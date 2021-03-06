#ifndef SimpleTTNDebug_h
#define SimpleTTNDebug_h

#include <sstream>

std::string describe(const u1_t *array, int length) {
    std::stringstream stream;
    stream << std::hex;
    for(int i=0; i<length; ++i) {
        unsigned int byte = array[i];
        if (byte == 0) {
            stream << "00";
        } else if (byte < 16) {
            stream << "0" << byte;
        } else {
            stream << byte;
        }
        // stream << " ";
    }
    return stream.str();
}

std::string describe(u4_t value) {
    return describe((u1_t *)&value, 4);
}

std::string describe(const std::vector<u1_t> &value) {
    return describe(value.data(), value.size());
}

static const char* const sEventNames[] = {LMIC_EVENT_NAME_TABLE__INIT};
std::string describe(ev_t event) {
    if (event < sizeof(sEventNames) / sizeof(sEventNames[0])) {
        return sEventNames[event];
    } else {
        return "EV_UNKNOWN";
    }
}

std::string describe(SimpleTTNState state) {
    switch(state) {
    case SimpleTTNStateIdle:
        return "idle";
    case SimpleTTNStateJoining:
        return "joining";
    case SimpleTTNStateJoinFailed:
        return "join_failed";
    case SimpleTTNStateReady:
        return "ready";
    case SimpleTTNStateTransceiving:
        return "transceiving";
    case SimpleTTNStateDisconnected:
        return "disconnected";
    }
}

#endif // Debug_h
