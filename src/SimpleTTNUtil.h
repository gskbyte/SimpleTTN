#ifndef SimpleTTNUtil_h
#define SimpleTTNUtil_h

#include <vector>
#include <sstream>

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

static std::vector<uint8_t> toBytes(std::string str) {
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

static uint32_t toInt32(std::string str) {
  if (str.length() != 8) {
    Serial.println("Invalid value for bin ");
    return 0;
  }

  uint32_t result = 0;
  const char *hex = str.c_str();
  while (*hex) {
      char c = *hex++; 
      uint8_t byte = hexCharToUInt8(c);
      // shift 4 to make space for new digit, and add the 4 bits of the new digit 
      result = (result << 4) | (byte & 0xF);
  }
  return result;
}

static std::vector<uint8_t> toBytes(uint32_t number) {
  std::vector<uint8_t> result(4);
  for (int i = 0; i < 4; i++)
      result[3 - i] = (number >> (i * 8));
  return result;
}

#endif // Util_h
