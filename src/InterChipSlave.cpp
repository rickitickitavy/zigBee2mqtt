#include "InterChipSlave.h"
#include "pins.h"
#include "Logger.h"

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

void InterChipSlave::resumeAfterRadioPause() {
    if (spiReady) {
        spi_slave_transaction_t *done = nullptr;
        while (spi_slave_get_trans_result(SPI2_HOST, &done, 0) == ESP_OK) {
        }
    }
    hardwareQueued = 0;
    fillSlot = 0;
    pumpPaused = false;
    LOGGER.info("SPI slave pump resumed after radio start");
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

void InterChipSlave::pumpDeviceDump() {
    if (!deviceDumpPending || deviceMapSource == nullptr) {
        return;
    }
    uint8_t payload[SPI_DEVICE_SYNC_ENTRY_LEN];
    if (!deviceDumpResetSent) {
        const int firstIndex = deviceMapSource->nextUsedIndex(0);
        uint8_t flags = SPI_DEVICE_SYNC_RESET;
        DeviceTopicEntry *entry = nullptr;
        if (firstIndex < 0) {
            flags |= SPI_DEVICE_SYNC_LAST;
            const size_t length = DeviceTopicMap::packSyncPayload(payload, sizeof(payload), flags, nullptr);
            if (length > 0 && enqueueEvent(SpiEvtDeviceMap, payload, (uint16_t)length)) {
                deviceDumpPending = false;
            }
            return;
        }
        flags |= SPI_DEVICE_SYNC_ENTRY;
        entry = deviceMapSource->slotAt(firstIndex);
        if (deviceMapSource->nextUsedIndex(firstIndex + 1) < 0) {
            flags |= SPI_DEVICE_SYNC_LAST;
        }
        const size_t length = DeviceTopicMap::packSyncPayload(payload, sizeof(payload), flags, entry);
        if (length == 0 || !enqueueEvent(SpiEvtDeviceMap, payload, (uint16_t)length)) {
            return;
        }
        deviceDumpResetSent = true;
        deviceDumpIndex = firstIndex + 1;
        if ((flags & SPI_DEVICE_SYNC_LAST) != 0) {
            deviceDumpPending = false;
        }
        return;
    }

    const int slotIndex = deviceMapSource->nextUsedIndex(deviceDumpIndex);
    uint8_t flags = 0;
    DeviceTopicEntry *entry = nullptr;
    if (slotIndex < 0) {
        flags = SPI_DEVICE_SYNC_LAST;
    } else {
        flags = SPI_DEVICE_SYNC_ENTRY;
        entry = deviceMapSource->slotAt(slotIndex);
        if (deviceMapSource->nextUsedIndex(slotIndex + 1) < 0) {
            flags |= SPI_DEVICE_SYNC_LAST;
        }
    }
    const size_t length = DeviceTopicMap::packSyncPayload(payload, sizeof(payload), flags, entry);
    if (length == 0 || !enqueueEvent(SpiEvtDeviceMap, payload, (uint16_t)length)) {
        return;
    }
    if (slotIndex >= 0) {
        deviceDumpIndex = slotIndex + 1;
    }
    if ((flags & SPI_DEVICE_SYNC_LAST) != 0) {
        deviceDumpPending = false;
    }
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

bool InterChipSlave::tryEnqueue(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t length) {
    if (length > SPI_MAX_PAYLOAD) {
        return false;
    }
    for (int i = 0; i < kQueue; i++) {
        if (outbound[i].used) {
            continue;
        }
        outbound[i].used = true;
        outbound[i].frame.cmd = cmd;
        if (seq != 0) {
            outbound[i].frame.seq = seq;
        } else {
            outbound[i].frame.seq = nextSeq++;
            if (nextSeq == 0) {
                nextSeq = 1;
            }
        }
        outbound[i].frame.length = length;
        if (length > 0 && payload != nullptr) {
            memcpy(outbound[i].frame.payload, payload, length);
        }
        updateIrq();
        return true;
    }
    return false;
}

void InterChipSlave::dropOldestLogRecord() {
    for (int i = 0; i < kQueue; i++) {
        if (outbound[i].used && outbound[i].frame.cmd == SpiEvtLogRecord) {
            outbound[i].used = false;
            updateIrq();
            return;
        }
    }
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
        for (int i = 0; i < kQueue; i++) {
            if (outbound[i].used && outbound[i].frame.cmd == SpiEvtLogRecord) {
                outbound[i].used = false;
                break;
            }
        }
        enqueueEvent(SpiEvtLogRecord, (const uint8_t *)line, (uint16_t)length);
    }
}

void InterChipSlave::enqueueAttrReport(bool on, const uint8_t ieee[8], uint8_t endpoint, uint16_t shortAddr) {
    uint8_t payload[12];
    memcpy(payload, ieee, 8);
    payload[8] = endpoint;
    payload[9] = (uint8_t)(shortAddr & 0xFF);
    payload[10] = (uint8_t)((shortAddr >> 8) & 0xFF);
    payload[11] = on ? 1 : 0;
    enqueueEvent(SpiEvtAttrReport, payload, 12);
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
    bool hasEvent = false;
    for (int i = 0; i < kQueue; i++) {
        if (outbound[i].used) {
            hasEvent = true;
            break;
        }
    }
    pinMode(PIN_SPI_IRQ, OUTPUT);
    digitalWrite(PIN_SPI_IRQ, hasEvent ? HIGH : LOW);
}

bool InterChipSlave::takeOutbound(SpiFrame &frame) {
    for (int i = 0; i < kQueue; i++) {
        if (!outbound[i].used) {
            continue;
        }
        frame = outbound[i].frame;
        outbound[i].used = false;
        updateIrq();
        return true;
    }
    return false;
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
    if (frame.cmd == SpiCmdZclOnOff && frame.length >= 9) {
        memcpy(deferredOnOffIeee, frame.payload, 8);
        deferredOnOffAction = frame.payload[8];
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
        deviceDumpPending = true;
        deviceDumpResetSent = false;
        deviceDumpIndex = 0;
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
        onOffHandler(deferredOnOffIeee, deferredOnOffAction);
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
