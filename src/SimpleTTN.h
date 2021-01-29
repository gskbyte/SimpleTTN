#ifndef SimpleTTN_h
#define SimpleTTN_h

#include "Arduino.h"
#include "lmic/lmic.h"
#include "lmic/arduino_lmic_hal_boards.h"

enum SimpleTTNState {
    SimpleTTNStateIdle,

    SimpleTTNStateJoining,
    SimpleTTNStateJoinFailed,

    SimpleTTNStateReady,

    SimpleTTNStateTransceiving,

    SimpleTTNStateDisconnected
};

// For more information: http://wiki.lahoud.fr/lib/exe/fetch.php?media=lmic-v1.5.pdf
struct SimpleTTNConfiguration {
    // Periodically checks whether a connection is established.
    // Defaults to false because of incomplete support (default true in LMIC);
    bool linkCheckEnabled = false;
};

class SimpleTTN {
public:
    static SimpleTTN *instance();
    static SimpleTTN *initialize();
    static SimpleTTN *initialize(const TTN_esp32_LMIC::HalPinmap_t* pinmap);

private:
    // Private constructor: class must be initialized via singleton methods.
    SimpleTTN();

public:
    SimpleTTNState state();

    // TODO add retry support
    // returns false if keys are bad
    bool ready();
    void configure(SimpleTTNConfiguration configuration = SimpleTTNConfiguration());
    bool provisionOTAA(std::string devEui, std::string appEui, std::string appKey);
    bool provisionABP(std::string deviceAddress, std::string networkKey, 
                       std::string appSessionKey, u4_t sequenceNumberUp = 0);
    bool join();
    void stop();

    bool poll(uint8_t port, bool confirm = false);
    bool send(const std::vector<uint8_t> &message, uint8_t port, bool confirm = false);

    void onMessage(void (*callback)(const std::vector<uint8_t> &payload, int rssi));

    // TODO configure transmission power, data rate,
    // TODO getters for mac, frequency, etc

    // OTAA parameters
    std::string deviceEUI() const;
    std::string appEUI() const;
    std::string appKey() const;

    // ABP and session parameters
    std::string deviceAddress() const;
    std::string networkKey() const;
    std::string appSessionKey() const;
    uint32_t sequenceNumberUp() const;

    std::string statusDescription();

protected:
    void handleEvent_JOINING();
    void handleEvent_JOINED();
    void handleEvent_JOIN_TXCOMPLETE();
    void handleEvent_JOIN_FAILED();
    void handleEvent_TXSTART();
    void handleEvent_TXCOMPLETE();

    // OTAA activation
    std::vector<uint8_t> _devEui;
    std::vector<uint8_t> _appEui;
    std::vector<uint8_t> _appKey;

    // ABP / session
    std::vector<uint8_t> _deviceAddress;
    std::vector<uint8_t> _networkKey;
    std::vector<uint8_t> _appSessionKey;
    uint32_t _sequenceNumberUp;

    SimpleTTNConfiguration _configuration;

    SimpleTTNState _state;
    std::vector<uint8_t> _pendingMessage;
    void (*_messageCallback)(const std::vector<uint8_t> &payload, int rssi) = nullptr;
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

#endif // SimpleTTN_h
