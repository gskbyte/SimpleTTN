#include "TTNDevice.h"

#include "Debug.h"
#include "Util.h"

#include "lmic/lmic/oslmic.h"
#include <ArduinoLog.h>

/// Static stuff

static TTNDevice *sInstance = nullptr;

TTNDevice *TTNDevice::instance() {
    return sInstance;
}

TTNDevice *TTNDevice::initialize() {
    return initialize(TTN_esp32_LMIC::GetPinmap_ThisBoard());
}

TTNDevice *TTNDevice::initialize(const TTN_esp32_LMIC::HalPinmap_t* pinmap) {
    Log.trace("Initializing");
    if (sInstance) {
        Log.warning("Global TTNDevice object existed, being replaced");
        delete sInstance;
    }

    bool success = os_init_ex(pinmap);
    if (success) {
        // Reset the MAC state. Session and pending data transfers will be discarded.
        LMIC_reset();
        sInstance = new TTNDevice();
    } else {
        Log.fatal("Couldn't initialize device, check pinmap.");
    }
    return sInstance;
}

/// Instance functions

TTNDevice::TTNDevice() {
    _sequenceNumberUp = 0;
    _state = TTNDeviceStateIdle;
    configure(TTNDeviceConfiguration());
}

TTNDeviceState TTNDevice::state() {
    return _state;
}

void TTNDevice::configure(TTNDeviceConfiguration configuration) {
    _configuration = configuration;

    // TODO set LMIC properties and allow configuring them

    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;
    // Set data rate and transmit power for uplink
    LMIC_setDrTxpow(DR_SF7, 14);
    // Allow 7% clock errors
    LMIC_setClockError(MAX_CLOCK_ERROR * 7 / 100);
}

bool TTNDevice::provisionOTAA(std::string devEui, std::string appEui, std::string appKey) {
    // TODO check keys, return false if invalid

    _devEui = toBytes(devEui);
    _appEui = toBytes(appEui);
    _appKey = toBytes(appKey);

    return true;
}

bool TTNDevice::join() {
    Log.trace("Joining");
    
    LMIC_unjoin();
    LMIC_startJoining();
    
    this->startLoop();
    
    _state = TTNDeviceStateJoining;
    return true;
}

bool TTNDevice::provisionABP(std::string deviceAddress, std::string networkKey, 
                     std::string appSessionKey, u4_t sequenceNumberUp) {
    // TODO check keys
    _deviceAddress = toBytes(deviceAddress);
    _networkKey = toBytes(networkKey);
    _appSessionKey = toBytes(appSessionKey);
    _sequenceNumberUp = sequenceNumberUp;

    // 0x13 is the net ID for TTN
    uint32_t swappedAddress = _deviceAddress[0] << 24 | _deviceAddress[1] << 16 | _deviceAddress[2] << 8 | _deviceAddress[3];
    LMIC_setSession(0x13, swappedAddress, _networkKey.data(), _appSessionKey.data());
#if defined(CFG_eu868)
    // Set up the channels used by the Things Network, which corresponds
    // to the defaults of most gateways. Without this, only three base
    // channels from the LoRaWAN specification are used, which certainly
    // works, so it is good for debugging, but can overload those
    // frequencies, so be sure to configure the full frequency range of
    // your network here (unless your network autoconfigures them).
    // Setting up channels should happen after LMIC_setSession, as that
    // configures the minimal channel set.
    // NA-US channels 0-71 are configured automatically
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B),
        BAND_CENTI); // g-band
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK),
        BAND_MILLI); // g2-band
#endif


    LMIC_setLinkCheckMode(0);
    LMIC.dn2Dr = DR_SF9;
    LMIC_setDrTxpow(DR_SF7, 14);
    LMIC_setSeqnoUp(sequenceNumberUp);

    this->startLoop();
    _state = TTNDeviceStateReady;
    return true;
}

void TTNDevice::stop() {
    Log.trace("Stopping");
    if (_taskHandle != nullptr) {
        LMIC_reset();
        this->stopLoop();
        _taskHandle = nullptr;
    }
    _state = TTNDeviceStateIdle;
}

bool TTNDevice::poll(uint8_t port, bool confirm) {
    Log.trace("Polling on port %i", port);
    if (_state != TTNDeviceStateReady) {
        Log.error("Can't poll in state %s", describe(_state).c_str());
        return false;
    }
    if (LMIC.opmode & OP_TXRXPEND) {
        Log.error("LMIC indicates there's a pending transaction, cancelling poll.");
        return false;
    }

    LMIC_setTxData2(port, nullptr, 1, confirm ? 1 : 0);
    return true;
}

bool TTNDevice::send(std::vector<uint8_t> message, uint8_t port, bool confirm) {
    Log.trace("Sending data on port %i: %s", port, describe(message).c_str());

    if (_state != TTNDeviceStateReady) {
        Log.error("Can't send data in state %s", describe(_state).c_str());
        return false;
    }

    if (LMIC.opmode & OP_TXRXPEND) {
        Log.error("LMIC indicates there's a pending transaction, cancelling send.");
        return false;
    }
    
    _pendingMessage = message;
    _state = TTNDeviceStateTransceiving;

    LMIC_setTxData2(port, _pendingMessage.data(), _pendingMessage.size(), confirm ? 1 : 0);
    return true;
}

// void TTNDevice::sendMessageAsync(std::vector<uint8_t> data) {

// }

// void TTNDevice::onMessage(void (*callback)(const uint8_t* payload, size_t size, int rssi)) {
    
// }

std::string TTNDevice::deviceEUI() const {
    return describe(_devEui);
}

std::string TTNDevice::appEUI() const {
    return describe(_appEui);
}

std::string TTNDevice::appKey() const {
    return describe(_appKey);
}

std::string TTNDevice::deviceAddress() const {
    return describe(_deviceAddress);
}

std::string TTNDevice::networkKey() const {
    return describe(_networkKey);
}

std::string TTNDevice::appSessionKey() const {
    return describe(_appSessionKey);
}

uint32_t TTNDevice::sequenceNumberUp() const {
    return _sequenceNumberUp;
}

std::string TTNDevice::statusDescription() {
    std::stringstream stream;

    stream << "-----------------------------" << std::endl;

    stream << "State: " << describe(_state) << std::endl;
    if (_pendingMessage.size() > 0) {
        stream << "Pending message: " << describe(_pendingMessage) << std::endl;
    }
    stream << "DevEUI: " << this->deviceEUI() << std::endl;
    stream << "AppEUI: " << this->appEUI() << std::endl;
    stream << "AppKey: " << this->appKey() << std::endl;

    stream << "LMIC deviceAddress: " << this->deviceAddress() << std::endl;
    stream << "LMIC networkKey: " << this->networkKey() << std::endl;
    stream << "LMIC appSessionKey: " << this->appSessionKey() << std::endl;
    stream << "LMIC seqNumUp: " << this->sequenceNumberUp() << std::endl;

    // stream << std::dec;
    // stream << "LMIC dataRate: " << _lmic_devAddr << std::endl;
    // stream << "LMIC txPower: " << _lmic_devAddr << " dB" << std::endl;
    // stream << "LMIC freq: " <<  _lmic_devAddr << " Hz" << std::endl;

    stream << "-----------------------------";

    return stream.str();
}

/// PRIVATE

void TTNDevice::handleEvent_JOINING() {
    Log.trace("handleEvent_JOINING");
    // TODO log joining
    _state = TTNDeviceStateJoining;
}

void TTNDevice::handleEvent_JOINED() {
    Log.trace("handleEvent_JOINED");

    u4_t netId;
    devaddr_t devAddr;
    u1_t nwkKey[16];
    u1_t artKey[16];
    LMIC_getSessionKeys(&netId, &devAddr, nwkKey, artKey);

    _deviceAddress = { (u1_t)(devAddr >> 24),
        (u1_t)(devAddr >> 16), 
        (u1_t)(devAddr >> 8), 
        (u1_t)(devAddr)};
    _networkKey.assign(nwkKey, nwkKey+16);
    _appSessionKey.assign(artKey, artKey+16);

    LMIC_setLinkCheckMode(_configuration.linkCheckEnabled ? 1 : 0);

    // TODO log current state
    _state = TTNDeviceStateReady;
}

void TTNDevice::handleEvent_JOIN_TXCOMPLETE() {
    Log.trace("handleEvent_JOIN_TXCOMPLETE");
    Log.warning("Waiting to join - Reuse previous keys if possible");
}

void TTNDevice::handleEvent_JOIN_FAILED() {
    Log.trace("handleEvent_JOIN_FAILED");
    // TODO Log error
    _state = TTNDeviceStateJoinFailed;
}

void TTNDevice::handleEvent_TXSTART() {
    Log.trace("handleEvent_TXSTART");

}

void TTNDevice::handleEvent_TXCOMPLETE() {
    Log.trace("handleEvent_TXCOMPLETE");
    
    _sequenceNumberUp = LMIC.seqnoUp;
    Log.notice("sequenceNumberUp: %i", _sequenceNumberUp);
    Log.notice("txrxFlags: %b", LMIC.txrxFlags);
    if (LMIC.txrxFlags & TXRX_ACK) {
        Log.notice("Received ACK");
        // todo invoke callback for blocking send   
        // LMIC_clrTxData();
    }

    std::vector<u1_t> data;
    if (LMIC.dataLen) {
      u1_t offset = LMIC.dataBeg; // offset to get data.
      data.insert(data.end(), LMIC.frame + offset, LMIC.frame + offset + LMIC.dataLen);
      Log.trace("Received data (%i bytes, dataBeg %i): %s", LMIC.dataLen, LMIC.dataBeg, describe(data).c_str());
    }

    if (_state == TTNDeviceStateTransceiving) {
        _state = TTNDeviceStateReady;
        _pendingMessage = {};
    }

    // TODO: call general callback message

}

void TTNDevice::taskLoop(void* parameter) {
    for (;;) {
        os_runloop_once();
        vTaskDelay(16 / portTICK_PERIOD_MS); // delay 16 ms each run loop
    }
}

void TTNDevice::startLoop() {
    // TODO: Consider not pinned to core
    xTaskCreatePinnedToCore(taskLoop, "taskLoop", 2048, (void*)1, (5 | portPRIVILEGE_BIT), &_taskHandle, 1);
}

void TTNDevice::stopLoop() {
    vTaskDelete(_taskHandle);
    _taskHandle = 0;
}

/// LMIC Callbacks
void os_getArtEui(u1_t* buf) {
    std::vector<uint8_t> b = TTNDevice::instance()->_appEui;
    std::reverse(b.begin(),  b.end());  
    std::copy(b.begin(), b.end(), buf);
}

void os_getDevEui(u1_t* buf) {
    std::vector<uint8_t> b = TTNDevice::instance()->_devEui;
    std::reverse(b.begin(),  b.end());  
    std::copy(b.begin(), b.end(), buf);
}

void os_getDevKey(u1_t* buf) {
    std::vector<uint8_t> b = TTNDevice::instance()->_appKey;
    std::copy(b.begin(), b.end(), buf);
}

void onEvent(ev_t event) {
    TTNDevice *dev = TTNDevice::instance();
    switch(event) {
    case EV_JOINING:
        dev->handleEvent_JOINING();
        break;
    case EV_JOINED:
        dev->handleEvent_JOINED();
        break;
    case EV_JOIN_TXCOMPLETE:
        dev->handleEvent_JOIN_TXCOMPLETE();
        break;
    case EV_JOIN_FAILED:
        dev->handleEvent_JOIN_FAILED();
        break;
    case EV_TXSTART:
        dev->handleEvent_TXSTART();
        break;
    case EV_TXCOMPLETE:
        dev->handleEvent_TXCOMPLETE();
        break;
    case EV_RXCOMPLETE:
        // break;
    case EV_RESET:
        // break;
    case EV_LINK_DEAD:
        // break;
    default:
        Log.trace("Unhandled event: %s", describe(event).c_str());
        break;
    }
}