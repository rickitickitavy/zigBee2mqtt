#include "InterChipSlave.h"
#include "pins.h"
#include "Logger.h"

#include <driver/spi_slave.h>
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
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static uint8_t dmaTx[SPI_MAX_FRAME] __attribute__((aligned(4)));
static uint8_t dmaRx[SPI_MAX_FRAME] __attribute__((aligned(4)));
static spi_slave_transaction_t slaveTransDesc;
static bool transQueued = false;

void interChipSlaveLogHook(const char *line) {
    INTER_CHIP_SLAVE.enqueueLogLine(line);
}

void InterChipSlave::begin() {
    pinMode(PIN_SPI_IRQ, OUTPUT);
    digitalWrite(PIN_SPI_IRQ, LOW);

    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = PIN_SPI_MOSI;
    busConfig.miso_io_num = PIN_SPI_MISO;
    busConfig.sclk_io_num = PIN_SPI_SCK;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.max_transfer_sz = SPI_MAX_FRAME;

    spi_slave_interface_config_t slaveConfig = {};
    slaveConfig.spics_io_num = PIN_SPI_CS;
    slaveConfig.flags = 0;
    slaveConfig.queue_size = 3;
    slaveConfig.mode = 0;

    const esp_err_t err = spi_slave_initialize(SPI2_HOST, &busConfig, &slaveConfig, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        LOGGER.error("SPI slave init failed");
        return;
    }
    spiReady = true;
    memset(dmaTx, 0, sizeof(dmaTx));
    memset(dmaRx, 0, sizeof(dmaRx));
    xTaskCreate(slaveSpiTask, "slaveSpi", 4096, nullptr, 1, nullptr);
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

bool InterChipSlave::enqueueReply(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t length) {
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
    if (frame.cmd == SpiCmdPermitJoin && frame.length >= 1 && permitJoinHandler != nullptr) {
        permitJoinHandler(frame.payload[0]);
        uint8_t ok = 1;
        enqueueEvent(SpiEvtCmdResult, &ok, 1);
        return;
    }
    if (frame.cmd == SpiCmdZclOnOff && frame.length >= 9 && onOffHandler != nullptr) {
        onOffHandler(frame.payload, frame.payload[8]);
        uint8_t ok = 1;
        enqueueEvent(SpiEvtCmdResult, &ok, 1);
        return;
    }
}

void InterChipSlave::serviceSpi() {
    if (!spiReady) {
        return;
    }
    if (!transQueued) {
        memset(dmaTx, 0, sizeof(dmaTx));
        memset(dmaRx, 0, sizeof(dmaRx));
        SpiFrame outgoing;
        if (takeOutbound(outgoing)) {
            spiEncodeFrame(outgoing, dmaTx, sizeof(dmaTx));
        }
        memset(&slaveTransDesc, 0, sizeof(slaveTransDesc));
        slaveTransDesc.length = SPI_MAX_FRAME * 8;
        slaveTransDesc.tx_buffer = dmaTx;
        slaveTransDesc.rx_buffer = dmaRx;
        if (spi_slave_queue_trans(SPI2_HOST, &slaveTransDesc, 0) == ESP_OK) {
            transQueued = true;
        }
        return;
    }

    spi_slave_transaction_t *done = nullptr;
    if (spi_slave_get_trans_result(SPI2_HOST, &done, 0) != ESP_OK) {
        return;
    }
    transQueued = false;
    SpiFrame inbound;
    if (spiDecodeFrame(dmaRx, SPI_MAX_FRAME, inbound)) {
        handleHostFrame(inbound);
    }
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

void InterChipSlave::pump() {
    if (!readySent && spiReady) {
        readySent = true;
        enqueueEvent(SpiEvtSlaveReady, nullptr, 0);
        LOGGER.info("SLAVE_READY queued");
    }
    serviceSpi();
}
