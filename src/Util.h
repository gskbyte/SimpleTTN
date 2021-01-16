#ifndef Util_H
#define Util_h

#include <vector>
#include <sstream>

#define STTN_LOG_LEVEL_ERROR   0
#define STTN_LOG_LEVEL_WARNING 1
#define STTN_LOG_LEVEL_DEBUG   2

static uint8_t hexCharToUInt8(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'A' && ch <= 'F') {
        return ch + 10 - 'A';
    } else if (ch >= 'a' && ch <= 'f') {
        return ch + 10 - 'a';
    }
    return UINT8_MAX;
}

std::vector<uint8_t> hexStrToBin(std::string str) {
  if (str.length() == 0 || str.length()%2!=0) {
    Serial.println("Invalid value for bin ");
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> result;
  const char *ptr = str.c_str();
  for (int i=0; i<str.length(); i+=2) {
    uint8_t first = hexCharToUInt8(str.c_str()[i]);
    uint8_t second = hexCharToUInt8(str.c_str()[i+1]);
    if (first == UINT8_MAX || second == UINT8_MAX) {
      Serial.print("Invalid value for bin ");
      return std::vector<uint8_t>();
    }
    result.push_back( (first<<4)|second );
  }
  return result;
}

#endif // Util_h
