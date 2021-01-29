#include <SimpleTTN.h>
#include <ArduinoLog.h>

bool useOTAA = true;

// Change if using OTAA
const char* otaaDevEui = "CHANGE_ME";
const char* otaaAppEui = "CHANGE_ME";
const char* otaaAppKey = "CHANGE_ME";

// Change if using ABP or resuming session
const char* abpDevAddress = "CHANGE_ME";
const char* abpNetworkKey = "CHANGE_ME";
const char* abpSessionKey = "CHANGE_ME";

void printNewline(Print* _logOutput) {
  _logOutput->println();
}
 
SimpleTTN *dev;
bool resumeLastSession = false;

void setup() {
  Serial.begin(115200);
  while(!Serial);
  delay(500);
  
  Log.begin(LOG_LEVEL_VERBOSE, &Serial, true);
  Log.setSuffix(printNewline);
 
  dev = SimpleTTN::initialize();

  if (useOTAA) {
    dev->provisionOTAA(otaaDevEui, otaaAppEui, otaaAppKey);
  } else {
    dev->provisionABP(abpDevAddress, abpNetworkKey, abpSessionKey);
  }

  dev->join();

  while(dev->state() == SimpleTTNStateJoining);
  Serial.println("Joined!");
  Serial.println(dev->statusDescription().c_str());
  delay(1000);
}
 
u1_t val = 1;
void loop() {
  if (dev->state() == SimpleTTNStateReady) {
    if (val % 2 == 1) {
      Serial.println("Sending");
      dev->send({val, val, val}, 2);
    } else {
      Serial.println("Polling");
      dev->poll(2);
    }
  }
  
  ++val;
  delay(10 * 1000);
}
