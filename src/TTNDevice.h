#ifndef TTNDevice_h
#define TTNDevice_h

#include "Arduino.h"
#include "lmic/lmic.h"
#include "lmic/arduino_lmic_hal_boards.h"

enum TTNDeviceState {
    TTNDeviceStateIdle,

    TTNDeviceStateJoining,
    TTNDeviceStateJoinFailed,

    TTNDeviceStateReady,

    TTNDeviceStateTransceiving,

    TTNDeviceStateDisconnected
};

// For more information: http://wiki.lahoud.fr/lib/exe/fetch.php?media=lmic-v1.5.pdf
struct TTNDeviceConfiguration {
    // Periodically checks whether a connection is established.
    // Defaults to false because of incomplete support (default true in LMIC);
    bool linkCheckEnabled = false;
};

class TTNDevice {
public:
    static TTNDevice *instance();
    static TTNDevice *initialize();
    static TTNDevice *initialize(const TTN_esp32_LMIC::HalPinmap_t* pinmap);

private:
    // Private constructor: class must be initialized via singleton methods.
    TTNDevice();

public:
    TTNDeviceState state();

    // TODO add retry support
    // returns false if keys are bad
    bool ready();
    bool join(std::string devEui, std::string appEui, std::string appKey, TTNDeviceConfiguration configuration = TTNDeviceConfiguration());
    void stop();

    bool poll(uint8_t port);
    bool send(std::vector<uint8_t> message, uint8_t port);
    // void sendMessageAsync(std::vector<uint8_t> data, uint8_t port);
    // void onMessage(void (*callback)(const uint8_t* payload, size_t size, int rssi));

    std::string statusDescription();

protected:
    // Loop function for the TTN task.
    static void taskLoop(void* parameter);

    bool checkKeys();

    void handleEvent_JOINING();
    void handleEvent_JOINED();
    void handleEvent_JOIN_FAILED();
    void handleEvent_TXSTART();
    void handleEvent_TXCOMPLETE();

    std::string _devEui;
    std::string _appEui;
    std::string _appKey;
    TTNDeviceConfiguration _configuration;

    TTNDeviceState _state;
    std::vector<uint8_t> _pendingMessage;
private:
    TaskHandle_t _taskHandle;

    // LMIC variables and functions
    u4_t _lmic_netId;
    devaddr_t _lmic_devAddr;
    u1_t _lmic_nwkKey[16];
    u1_t _lmic_artKey[16];

    // Global LMIC functions need to be friend so they can access private fields.
    friend void os_getArtEui(u1_t* buf);
    friend void os_getDevEui(u1_t* buf);
    friend void os_getDevKey(u1_t* buf);
    friend void onEvent(ev_t event);
};

#endif // TTNDevice_h
