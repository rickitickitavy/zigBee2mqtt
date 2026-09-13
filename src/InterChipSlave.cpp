#include "InterChipSlave.h"
#include "pins.h"
#include "Logger.h"
#include "StatusRgb.h"

#include <driver/spi_slave.h>
#include <esp_attr.h>
#include <esp_intr_alloc.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

InterChipSlave INTER_CHIP_SLAVE;

static void slaveSpiTask(void *arg) {
    (void)arg;
    for (;;) {
        INTER_CHIP_SLAVE.pump();
    }
}

static DMA_ATTR uint8_t dmaTx[2][SPI_MAX_FRAME] __attribute__((aligned(4)));
static DMA_ATTR uint8_t dmaRx[2][SPI_MAX_FRAME] __attribute__((aligned(4)));
static spi_slave_transaction_t slaveTransDesc[2];
static int fillSlot = 0;
static int hardwareQueued = 0;

void interChipSlaveLogHook(const char *line) {
    INTER_CHIP_SLAVE.enqueueLogLine(line);
}

bool InterChipSlave::initializeBus() {
    pinMode(PIN_SPI_IRQ, OUTPUT);
    digitalWrite(PIN_SPI_IRQ, LOW);

    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = PIN_SPI_MOSI;
    busConfig.miso_io_num = PIN_SPI_MISO;
    busConfig.sclk_io_num = PIN_SPI_SCK;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.max_transfer_sz = SPI_MAX_FRAME;
    busConfig.intr_flags = ESP_INTR_FLAG_LEVEL3;

    spi_slave_interface_config_t slaveConfig = {};
    slaveConfig.spics_io_num = PIN_SPI_CS;
    slaveConfig.flags = 0;
    slaveConfig.queue_size = 3;
    slaveConfig.mode = 0;

    // Frames are larger than the 64-byte CPU SPI buffer, so DMA is required.
    // Re-init after Zigbee.begin() so the radio cannot keep a dead DMA channel.
    const esp_err_t err = spi_slave_initialize(SPI2_HOST, &busConfig, &slaveConfig, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        LOGGER.error("SPI slave init failed");
        spiReady = false;
        STATUS_RGB.setCritical(true);
        return false;
    }
    spiReady = true;
    fillSlot = 0;
    hardwareQueued = 0;
    memset(dmaTx, 0, sizeof(dmaTx));
    memset(dmaRx, 0, sizeof(dmaRx));
    return true;
}

void InterChipSlave::setPumpPaused(bool paused) {
    pumpPaused = paused;
}

void InterChipSlave::requestDeviceDump() {
    if (deviceDumpPending) {
        return;
    }
    deviceDumpPending = true;
    deviceDumpHeaderSent = false;
    deviceDumpIndex = 0;
}

void InterChipSlave::resumeAfterRadioPause() {
    if (spiReady) {
        spi_slave_transaction_t *done = nullptr;
        while (spi_slave_get_trans_result(SPI2_HOST, &done, 0) == ESP_OK) {
        }
    }
    hardwareQueued = 0;
    fillSlot = 0;
    pumpPaused = false;
    requestDeviceDump();
    const int storedCount = deviceMapSource != nullptr ? deviceMapSource->usedCount() : 0;
    LOGGER.info("SPI slave pump resumed after radio start; sending " + String(storedCount) + " stored device(s)");
}

void InterChipSlave::begin() {
    if (!initializeBus()) {
        return;
    }
    xTaskCreate(slaveSpiTask, "slaveSpi", 4096, nullptr, 10, nullptr);
}

void InterChipSlave::setSettingsHandler(SettingsFn handler) {
    settingsHandler = handler;
}

void InterChipSlave::setPermitJoinHandler(PermitJoinFn handler) {
    permitJoinHandler = handler;
}

void InterChipSlave::setOnOffHandler(OnOffFn handler) {
    onOffHandler = handler;
}

void InterChipSlave::setDeviceSyncHandler(DeviceSyncFn handler) {
    deviceSyncHandler = handler;
}

void InterChipSlave::setDeviceMapSource(DeviceTopicMap *deviceMap) {
    deviceMapSource = deviceMap;
}

void InterChipSlave::setDevicesFileSource(DevicesFileFn handler) {
    devicesFileSource = handler;
}

void InterChipSlave::requestDevicesFileDump() {
    fileDumpPending = true;
    fileDumpStarted = false;
    fileDumpOffset = 0;
    fileDumpText = "";
}

void InterChipSlave::pumpDevicesFileDump() {
    if (!fileDumpPending) {
        return;
    }
    if (deviceDumpPending) {
        return;
    }
    if (!fileDumpStarted) {
        fileDumpText = devicesFileSource != nullptr ? devicesFileSource() : String("[]");
        fileDumpOffset = 0;
        fileDumpStarted = true;
    }
    uint8_t payload[SPI_MAX_PAYLOAD];
    const int totalLength = fileDumpText.length();
    const int remaining = totalLength - fileDumpOffset;
    const int chunkLength = remaining > (int)SPI_FILE_CHUNK_MAX ? (int)SPI_FILE_CHUNK_MAX : remaining;
    uint8_t flags = 0;
    if (fileDumpOffset == 0) {
        flags |= SPI_FILE_FIRST;
    }
    if (fileDumpOffset + chunkLength >= totalLength) {
        flags |= SPI_FILE_LAST;
    }
    payload[0] = flags;
    if (chunkLength > 0) {
        memcpy(payload + 1, fileDumpText.c_str() + fileDumpOffset, (size_t)chunkLength);
    }
    const uint16_t frameLength = (uint16_t)(1 + chunkLength);
    bool queued = tryEnqueue(SpiEvtDevicesFile, 0, payload, frameLength);
    if (!queued) {
        while (dropOldestLogRecord()) {
            queued = tryEnqueue(SpiEvtDevicesFile, 0, payload, frameLength);
            if (queued) {
                break;
            }
        }
    }
    if (!queued) {
        return;
    }
    fileDumpOffset += chunkLength;
    if ((flags & SPI_FILE_LAST) != 0) {
        fileDumpPending = false;
        fileDumpText = "";
    }
}

void InterChipSlave::pumpDeviceDump() {
    if (!deviceDumpPending || deviceMapSource == nullptr) {
        return;
    }
    uint8_t payload[SPI_DEVICE_SYNC_ENTRY_LEN];
    if (!deviceDumpHeaderSent) {
        payload[0] = SPI_DEVICE_SYNC_RESET;
        payload[1] = (uint8_t)deviceMapSource->usedCount();
        if (!enqueueDeviceMap(payload, 2)) {
            return;
        }
        deviceDumpHeaderSent = true;
        deviceDumpIndex = 0;
        LOGGER.info("Dumping " + String((int)payload[1]) + " device(s) to host");
    }

    while (true) {
        const int slotIndex = deviceMapSource->nextUsedIndex(deviceDumpIndex);
        if (slotIndex < 0) {
            break;
        }
        DeviceTopicEntry *entry = deviceMapSource->slotAt(slotIndex);
        const size_t length = DeviceTopicMap::packSyncPayload(
            payload,
            sizeof(payload),
            SPI_DEVICE_SYNC_ENTRY,
            entry
        );
        if (length == 0 || !enqueueDeviceMap(payload, (uint16_t)length)) {
            return;
        }
        deviceDumpIndex = slotIndex + 1;
    }

    const size_t lastLength = DeviceTopicMap::packSyncPayload(
        payload,
        sizeof(payload),
        SPI_DEVICE_SYNC_LAST,
        nullptr
    );
    if (lastLength == 0 || !enqueueDeviceMap(payload, (uint16_t)lastLength)) {
        return;
    }
    deviceDumpPending = false;
}

void InterChipSlave::applyHostTime(uint32_t unixSec) {
    if (unixSec < 1700000000) {
        return;
    }
    timeval tv{};
    tv.tv_sec = (time_t)unixSec;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
}

bool InterChipSlave::enqueueEvent(uint8_t cmd, const uint8_t *payload, uint16_t length) {
    return enqueueReply(cmd, 0, payload, length);
}

void InterChipSlave::removeOutboundAt(int index) {
    if (index < 0 || index >= outboundCount) {
        return;
    }
    if (index < outboundCount - 1) {
        memmove(
            &outbound[index],
            &outbound[index + 1],
            sizeof(QueuedFrame) * (size_t)(outboundCount - index - 1)
        );
    }
    outboundCount--;
    updateIrq();
}

bool InterChipSlave::tryEnqueue(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t length) {
    if (length > SPI_MAX_PAYLOAD || outboundCount >= kQueue) {
        return false;
    }
    QueuedFrame *slot = &outbound[outboundCount];
    memset(&slot->frame, 0, sizeof(slot->frame));
    slot->frame.cmd = cmd;
    if (seq != 0) {
        slot->frame.seq = seq;
    } else {
        slot->frame.seq = nextSeq++;
        if (nextSeq == 0) {
            nextSeq = 1;
        }
    }
    slot->frame.length = length;
    if (length > 0 && payload != nullptr) {
        memcpy(slot->frame.payload, payload, length);
    }
    outboundCount++;
    updateIrq();
    return true;
}

bool InterChipSlave::dropOldestLogRecord() {
    for (int i = 0; i < outboundCount; i++) {
        if (outbound[i].frame.cmd == SpiEvtLogRecord) {
            removeOutboundAt(i);
            return true;
        }
    }
    return false;
}

bool InterChipSlave::enqueueDeviceMap(const uint8_t *payload, uint16_t length) {
    if (tryEnqueue(SpiEvtDeviceMap, 0, payload, length)) {
        return true;
    }
    while (dropOldestLogRecord()) {
        if (tryEnqueue(SpiEvtDeviceMap, 0, payload, length)) {
            return true;
        }
    }
    return false;
}

bool InterChipSlave::enqueueReply(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t length) {
    if (tryEnqueue(cmd, seq, payload, length)) {
        return true;
    }
    dropOldestLogRecord();
    return tryEnqueue(cmd, seq, payload, length);
}

void InterChipSlave::enqueueLogLine(const char *line) {
    if (line == nullptr) {
        return;
    }
    const size_t length = strnlen(line, SPI_MAX_PAYLOAD);
    if (length == 0) {
        return;
    }
    if (!enqueueEvent(SpiEvtLogRecord, (const uint8_t *)line, (uint16_t)length)) {
        dropOldestLogRecord();
        enqueueEvent(SpiEvtLogRecord, (const uint8_t *)line, (uint16_t)length);
    }
}

void InterChipSlave::enqueueAttrReport(const char *message, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr) {
    uint8_t payload[11 + SPI_DEVICE_MESSAGE_MAX];
    memset(payload, 0, sizeof(payload));
    memcpy(payload, ieee, 8);
    payload[8] = endpoint;
    payload[9] = (uint8_t)(shortAddr & 0xFF);
    payload[10] = (uint8_t)((shortAddr >> 8) & 0xFF);
    const char *body = message != nullptr ? message : "";
    strncpy((char *)payload + 11, body, SPI_DEVICE_MESSAGE_MAX - 1);
    const uint16_t length = (uint16_t)(11 + strlen((char *)payload + 11) + 1);
    enqueueEvent(SpiEvtAttrReport, payload, length);
}

void InterChipSlave::enqueueDeviceJoin(
    const uint8_t ieee[8],
    uint16_t shortAddr,
    uint8_t endpoint,
    const char *manufacturer,
    const char *model
) {
    uint8_t payload[8 + 2 + 1 + 32 + 32];
    memset(payload, 0, sizeof(payload));
    memcpy(payload, ieee, 8);
    payload[8] = (uint8_t)(shortAddr & 0xFF);
    payload[9] = (uint8_t)((shortAddr >> 8) & 0xFF);
    payload[10] = endpoint;
    if (manufacturer != nullptr) {
        strncpy((char *)payload + 11, manufacturer, 31);
    }
    if (model != nullptr) {
        strncpy((char *)payload + 43, model, 31);
    }
    enqueueEvent(SpiEvtDeviceJoin, payload, sizeof(payload));
}

void InterChipSlave::updateIrq() {
    pinMode(PIN_SPI_IRQ, OUTPUT);
    digitalWrite(PIN_SPI_IRQ, outboundCount > 0 ? HIGH : LOW);
}

bool InterChipSlave::takeOutbound(SpiFrame &frame) {
    if (outboundCount <= 0) {
        return false;
    }
    frame = outbound[0].frame;
    removeOutboundAt(0);
    return true;
}

void InterChipSlave::handleHostFrame(const SpiFrame &frame) {
    if (frame.cmd == SpiCmdPing) {
        enqueueReply(SpiEvtPong, frame.seq, nullptr, 0);
        return;
    }
    if (frame.cmd == SpiCmdReadEvent || frame.cmd == SpiCmdGetStatus) {
        if (frame.cmd == SpiCmdGetStatus) {
            uint8_t status = readySent ? 1 : 0;
            enqueueReply(SpiEvtStatus, frame.seq, &status, 1);
        }
        return;
    }
    if (frame.cmd == SpiCmdSetSettings && frame.length >= 6) {
        pendingChannel = frame.payload[0];
        pendingPermitJoinSec = frame.payload[1];
        pendingUnixSec = (uint32_t)frame.payload[2] | ((uint32_t)frame.payload[3] << 8)
            | ((uint32_t)frame.payload[4] << 16) | ((uint32_t)frame.payload[5] << 24);
        applyHostTime(pendingUnixSec);
        settingsPending = true;
        enqueueReply(SpiEvtSettingsOk, frame.seq, nullptr, 0);
        return;
    }
    if (frame.cmd == SpiCmdTimeSync && frame.length >= 4) {
        const uint32_t unixSec = (uint32_t)frame.payload[0] | ((uint32_t)frame.payload[1] << 8)
            | ((uint32_t)frame.payload[2] << 16) | ((uint32_t)frame.payload[3] << 24);
        applyHostTime(unixSec);
        enqueueReply(SpiEvtPong, frame.seq, nullptr, 0);
        return;
    }
    if (frame.cmd == SpiCmdPermitJoin && frame.length >= 1) {
        deferredPermitSeconds = frame.payload[0];
        permitJoinPending = true;
        uint8_t ok = 1;
        enqueueEvent(SpiEvtCmdResult, &ok, 1);
        return;
    }
    if (frame.cmd == SpiCmdZclOnOff && frame.length >= 10) {
        memcpy(deferredOnOffIeee, frame.payload, 8);
        deferredOnOffEndpoint = frame.payload[8];
        memset(deferredOnOffCommand, 0, sizeof(deferredOnOffCommand));
        const size_t commandLength = frame.length - 9;
        const size_t bounded =
            commandLength >= sizeof(deferredOnOffCommand) ? sizeof(deferredOnOffCommand) - 1 : commandLength;
        memcpy(deferredOnOffCommand, frame.payload + 9, bounded);
        onOffPending = true;
        uint8_t ok = 1;
        enqueueEvent(SpiEvtCmdResult, &ok, 1);
        return;
    }
    if (frame.cmd == SpiCmdSetDevice && frame.length >= 1 && deviceSyncHandler != nullptr) {
        uint8_t flags = 0;
        DeviceTopicEntry entry;
        if (!DeviceTopicMap::unpackSyncPayload(frame.payload, frame.length, &flags, &entry)) {
            return;
        }
        deviceSyncHandler(flags, &entry);
        return;
    }
    if (frame.cmd == SpiCmdGetDevices) {
        requestDeviceDump();
        return;
    }
    if (frame.cmd == SpiCmdGetDevicesFile) {
        requestDevicesFileDump();
        return;
    }
}

void InterChipSlave::fillHardwareQueue() {
    while (hardwareQueued < kHwSlots) {
        const int slot = fillSlot;
        memset(dmaTx[slot], 0, SPI_MAX_FRAME);
        memset(dmaRx[slot], 0, SPI_MAX_FRAME);
        SpiFrame outgoing;
        if (takeOutbound(outgoing)) {
            spiEncodeFrame(outgoing, dmaTx[slot], SPI_MAX_FRAME);
        }
        memset(&slaveTransDesc[slot], 0, sizeof(slaveTransDesc[slot]));
        slaveTransDesc[slot].length = SPI_MAX_FRAME * 8;
        slaveTransDesc[slot].tx_buffer = dmaTx[slot];
        slaveTransDesc[slot].rx_buffer = dmaRx[slot];
        if (spi_slave_queue_trans(SPI2_HOST, &slaveTransDesc[slot], 0) != ESP_OK) {
            return;
        }
        fillSlot = (fillSlot + 1) % kHwSlots;
        hardwareQueued++;
    }
}

void InterChipSlave::serviceSpi() {
    if (!spiReady) {
        vTaskDelay(pdMS_TO_TICKS(5));
        return;
    }
    fillHardwareQueue();

    spi_slave_transaction_t *done = nullptr;
    if (spi_slave_get_trans_result(SPI2_HOST, &done, pdMS_TO_TICKS(20)) != ESP_OK) {
        return;
    }
    if (hardwareQueued > 0) {
        hardwareQueued--;
    }
    if (done != nullptr && done->rx_buffer != nullptr) {
        SpiFrame inbound;
        if (spiDecodeFrame((const uint8_t *)done->rx_buffer, SPI_MAX_FRAME, inbound)) {
            handleHostFrame(inbound);
        }
    }
    fillHardwareQueue();
}

void InterChipSlave::applyDeferredSettings() {
    if (!settingsPending) {
        return;
    }
    settingsPending = false;
    if (settingsHandler != nullptr) {
        settingsHandler(pendingChannel, pendingPermitJoinSec, pendingUnixSec);
    }
}

void InterChipSlave::applyDeferredRadioCommands() {
    if (permitJoinPending && permitJoinHandler != nullptr) {
        if (permitJoinHandler(deferredPermitSeconds)) {
            permitJoinPending = false;
        }
    }
    if (onOffPending && onOffHandler != nullptr) {
        onOffHandler(deferredOnOffIeee, deferredOnOffCommand, deferredOnOffEndpoint);
        onOffPending = false;
    }
}

void InterChipSlave::pump() {
    if (pumpPaused) {
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
    }
    if (!readySent && spiReady) {
        readySent = true;
        enqueueEvent(SpiEvtSlaveReady, nullptr, 0);
        LOGGER.info("SLAVE_READY queued");
    }
    serviceSpi();
}
