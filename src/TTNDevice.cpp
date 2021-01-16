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
        sInstance = new TTNDevice();
    } else {
        Log.fatal("Couldn't initialize device, check pinmap.");
    }
    return sInstance;
}

/// Instance functions

TTNDevice::TTNDevice() {
    _state = TTNDeviceStateIdle;
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
}

TTNDeviceState TTNDevice::state() {
    return _state;
}

bool TTNDevice::join(std::string devEui, std::string appEui, std::string appKey,
                     TTNDeviceConfiguration configuration) {
    Log.trace("Joining");

    _devEui = devEui;
    _appEui = appEui;
    _appKey = appKey;
    _configuration = configuration;

    if (!checkKeys()) {
        Log.error("Provided keys for joining are invalid");
        return false;
    }
    
    LMIC_setClockError(MAX_CLOCK_ERROR * 50 / 100);
    LMIC_unjoin();
    LMIC_startJoining();

    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;
    // Set data rate and transmit power for uplink
    LMIC_setDrTxpow(DR_SF7, 14);

    // Consider not pinned to core
    xTaskCreatePinnedToCore(taskLoop, "taskLoop", 2048, (void*)1, (5 | portPRIVILEGE_BIT), &_taskHandle, 1);

    _state = TTNDeviceStateJoining;
    return true;
}

void TTNDevice::stop() {
    Log.trace("Stopping");
    if (_taskHandle != nullptr) {
        LMIC_reset();
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
    _state = TTNDeviceStateIdle;
}

bool TTNDevice::poll(uint8_t port) {
    Log.trace("Polling");
    if (_state != TTNDeviceStateReady) {
        Log.error("Can't poll in state %s", describe(_state).c_str());
        return false;
    }

    LMIC_setTxData2(port, 0, 1, 1);
}

bool TTNDevice::send(std::vector<uint8_t> message, uint8_t port) {
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

    LMIC_setTxData2(port, message.data(), (u1_t) message.size(), (u1_t)1);
}

// void TTNDevice::sendMessageAsync(std::vector<uint8_t> data) {

// }

// void TTNDevice::onMessage(void (*callback)(const uint8_t* payload, size_t size, int rssi)) {
    
// }

std::string TTNDevice::statusDescription() {
    std::stringstream stream;

    stream << "-----------------------------" << std::endl;

    stream << "State: " << describe(_state) << std::endl;
    if (_pendingMessage.size() > 0) {
        stream << "Pending message: " << describe(_pendingMessage) << std::endl;
    }
    stream << "DevEUI: " << _devEui << std::endl;
    stream << "AppEUI: " << _appEui << std::endl;
    stream << "AppKey: " << _appKey << std::endl;

    stream << std::hex;
    stream << "LMIC netID: " << std::hex << _lmic_netId << std::endl;
    stream << "LMIC devAddr: " << std::hex << _lmic_devAddr << std::endl;
    stream << "LMIC nwkKey: " << describe(_lmic_nwkKey, 16) << std::endl;
    stream << "LMIC artKey: " << describe(_lmic_artKey, 16) << std::endl;

    stream << std::dec;
    stream << "LMIC dataRate: " << _lmic_devAddr << std::endl;
    stream << "LMIC txPower: " << _lmic_devAddr << " dB" << std::endl;
    stream << "LMIC freq: " <<  _lmic_devAddr << " Hz" << std::endl;

    stream << "-----------------------------";

    return stream.str();
}

/// PRIVATE
void TTNDevice::taskLoop(void* parameter) {
    for (;;) {
        os_runloop_once();
        vTaskDelay(1 / portTICK_PERIOD_MS); // delay 1 ms each run loop
    }
}

bool TTNDevice::checkKeys() {
    // TODO
    return true;
}

void TTNDevice::handleEvent_JOINING() {
    Log.trace("handleEvent_JOINING");
    // TODO log joining
    _state = TTNDeviceStateJoining;
}

void TTNDevice::handleEvent_JOINED() {
    Log.trace("handleEvent_JOINED");
    LMIC_getSessionKeys(&_lmic_netId, &_lmic_devAddr, _lmic_nwkKey, _lmic_artKey);
    LMIC_setLinkCheckMode(_configuration.linkCheckEnabled ? 1 : 0);
    
    // TODO log current state
    _state = TTNDeviceStateReady;
}

void TTNDevice::handleEvent_JOIN_FAILED() {
    Log.trace("handleEvent_JOIN_FAILED");
    // TODO Log error
    _state = TTNDeviceStateJoinFailed;
}

void TTNDevice::handleEvent_TXSTART() {
    Log.trace("handleEvent_TXSTART");
    if (_state != TTNDeviceStateTransceiving) {
        /// Log with debug level, this is called for other things like joining
        Log.notice("TXSTART while not transceiving");
    }
}

void TTNDevice::handleEvent_TXCOMPLETE() {
    Log.trace("handleEvent_TXCOMPLETE");
    if (_state != TTNDeviceStateTransceiving) {
        /// Log with debug level, this is called for other things like joining
        Log.notice("TXCOMPLETE while not transceiving");
    }
    
    Log.notice("txrxFlags: %B", LMIC.txrxFlags);
    if (LMIC.txrxFlags & TXRX_ACK) {
        Log.notice("Received ACK");
        // todo invoke callback for blocking send   
        // LMIC_clrTxData();
    }

    std::vector<u1_t> data;
    if (LMIC.dataLen) {
      u1_t offset = 9;// offset to get data.
      data.insert(data.end(), LMIC.frame + offset, LMIC.frame + offset + LMIC.dataLen);
      Log.trace("Received data (%i bytes, dataBeg %i): %s", LMIC.dataLen, LMIC.dataBeg, describe(data).c_str());
    }

    if (_state == TTNDeviceStateTransceiving) {
        _state = TTNDeviceStateReady;
        _pendingMessage = {};
    }
        // TODO invoke callback

    // TODO: call general callback message
}


/// LMIC Callbacks
void os_getArtEui(u1_t* buf) {
    std::vector<uint8_t> b = hexStrToBin(TTNDevice::instance()->_appEui);
    std::reverse(b.begin(),  b.end());  
    std::copy(b.begin(), b.end(), buf);
}

void os_getDevEui(u1_t* buf) {
    std::vector<uint8_t> b = hexStrToBin(TTNDevice::instance()->_devEui);
    std::reverse(b.begin(),  b.end());  
    std::copy(b.begin(), b.end(), buf);
}

void os_getDevKey(u1_t* buf) {
    std::vector<uint8_t> b = hexStrToBin(TTNDevice::instance()->_appKey);
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
    case EV_JOIN_FAILED:
        dev->handleEvent_JOIN_FAILED();
        break;
    case EV_TXSTART:
        dev->handleEvent_TXSTART();
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