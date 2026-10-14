#include <Arduino.h>
#include <Bonezegei_WifiManager.h>

Bonezegei_WifiManager wifiManager;

// Variables
float temperature = 25.4;
bool ledState = false;
int pwmDuty = 128;
String nodeName = "Node-01";

#define LED_BUILTIN 2

// Callback triggered when ledState changes on portal
void onLedStateChange() {
  digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
  Serial.print("[Callback] LED State changed to: ");
  Serial.println(ledState ? "ON" : "OFF");
}

// Callback triggered when pwmDuty changes on portal
void onPwmChange() {
  Serial.print("[Callback] PWM changed to: ");
  Serial.println(pwmDuty);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  // 1. Automatic variable name + callback
  wifiManager.addVariable(ledState, onLedStateChange);

  // 2. Automatic variable name + writable + min + max + callback
  wifiManager.addVariable(pwmDuty, true, 0, 255, onPwmChange);

  // 3. Automatic variable name (Read-only variable, no callback needed)
  wifiManager.addVariable(temperature, false);

  // 4. Custom explicit string name + callback
  wifiManager.addVariable("Device Alias", nodeName, []() {
    Serial.print("[Callback] Name updated: ");
    Serial.println(nodeName);
  });

  //dashboard login
  wifiManager.setAuth("admin", "password123");


  wifiManager.begin("Bonezegei-Portal");

  // Once connected open browser and type http://bonezegei.local
}

void loop() {
  wifiManager.process();
}