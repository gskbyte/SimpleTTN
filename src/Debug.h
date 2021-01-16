#ifndef Util_H
#define Util_h

#include <sstream>

std::string describe(std::vector<u1_t> value) {
    std::stringstream stream;
    stream << std::hex;
    for(int i=0; i<value.size(); ++i) {
        stream << (int) value[i] << " ";
    }
    return stream.str();
}

std::string describe(u1_t *array, int length) {
    std::stringstream stream;
    stream << std::hex;
    for(int i=0; i<length; ++i) {
        stream << (int) array[i] << " ";
    }
    return stream.str();
}

static const char* const sEventNames[] = {LMIC_EVENT_NAME_TABLE__INIT};
std::string describe(ev_t event) {
    if (event < sizeof(sEventNames) / sizeof(sEventNames[0])) {
        return sEventNames[event];
    } else {
        return "EV_UNKNOWN";
    }
}

std::string describe(TTNDeviceState state) {
    switch(state) {
    case TTNDeviceStateIdle:
        return "idle";
    case TTNDeviceStateJoining:
        return "joining";
    case TTNDeviceStateJoinFailed:
        return "join_failed";
    case TTNDeviceStateReady:
        return "ready";
    case TTNDeviceStateTransceiving:
        return "transceiving";
    case TTNDeviceStateDisconnected:
        return "disconnected";
    }
}

#endif // Util_h
