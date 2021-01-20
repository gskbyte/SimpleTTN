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
    void configure(TTNDeviceConfiguration configuration = TTNDeviceConfiguration());
    bool provision(std::string devEui, std::string appEui, std::string appKey);
    bool join();
    bool resumeSession(std::string deviceAddress, std::string networkKey, 
                       std::string appSessionKey, u4_t sequenceNumberUp);
    void stop();

    bool poll(uint8_t port, bool confirm = false);
    bool send(std::vector<uint8_t> message, uint8_t port, bool confirm = false);

    // TODO
    // void onMessage(void (*callback)(const uint8_t* payload, size_t size, int rssi));

    // TODO add getters for keys and various states
    // TODO configure transmission power, data rate,
    // TODO getters for mac, frequency, etc

    std::string statusDescription();

protected:
    void handleEvent_JOINING();
    void handleEvent_JOINED();
    void handleEvent_JOIN_FAILED();
    void handleEvent_TXSTART();
    void handleEvent_TXCOMPLETE();

    // OTAA activation
    std::vector<uint8_t> _devEui;
    std::vector<uint8_t> _appEui;
    std::vector<uint8_t> _appKey;

    // Session state
    u4_t _deviceAddress;
    std::vector<uint8_t> _networkKey;
    std::vector<uint8_t> _appSessionKey;
    u4_t _sequenceNumberUp;

    TTNDeviceConfiguration _configuration;

    TTNDeviceState _state;
    std::vector<uint8_t> _pendingMessage;
private:
    // Loop function for the TTN task.
    static void taskLoop(void* parameter);
    void startLoop();
    void stopLoop();
    // Task handle to manage the TTN task.
    TaskHandle_t _taskHandle;

    // Global LMIC functions need to be friend so they can access private fields.
    friend void os_getArtEui(u1_t* buf);
    friend void os_getDevEui(u1_t* buf);
    friend void os_getDevKey(u1_t* buf);
    friend void onEvent(ev_t event);
};

#endif // TTNDevice_h
