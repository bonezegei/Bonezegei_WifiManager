#ifndef BONEZEGEI_WIFIMANAGER_H
#define BONEZEGEI_WIFIMANAGER_H

#include <Arduino.h>
#include <functional>
#include <type_traits>

#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <DNSServer.h>
  #include <Preferences.h>
  #include <ESPmDNS.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <DNSServer.h>
  #include <EEPROM.h>
  #include <ESP8266mDNS.h>
  using WebServer = ESP8266WebServer;
#endif

typedef std::function<void()> VariableCallback;

enum VarType {
  VAR_INT,
  VAR_FLOAT,
  VAR_BOOL,
  VAR_STRING
};

struct ManagedVariable {
  String name;
  void* ptr;
  VarType type;
  bool isWritable;
  float minVal;
  float maxVal;
  VariableCallback callback;
};

struct SavedCredential {
  String ssid;
  String pass;
};

class Bonezegei_WifiManager {
public:
  Bonezegei_WifiManager();
  
  void setAuth(const char* username, const char* password);
  void begin(const char* apSSID = "Bonezegei-AP", const char* apPassword = "", bool keepAPAlive = true);
  void process();
  
  // --- Core Overloaded Variable Registration ---
  void addVariable(const String& name, bool& var, bool writable = true, VariableCallback cb = nullptr);
  void addVariable(const String& name, String& var, bool writable = true, VariableCallback cb = nullptr);

  void addVariable(const String& name, int& var, bool writable = true, VariableCallback cb = nullptr);
  void addVariable(const String& name, int& var, bool writable, int minVal, int maxVal, VariableCallback cb = nullptr);

  void addVariable(const String& name, float& var, bool writable = true, VariableCallback cb = nullptr);
  void addVariable(const String& name, float& var, bool writable, float minVal, float maxVal, VariableCallback cb = nullptr);

  template <typename U>
  struct is_callable : std::integral_constant<bool,
    !std::is_same<typename std::decay<U>::type, bool>::value &&
    !std::is_same<typename std::decay<U>::type, int>::value &&
    !std::is_same<typename std::decay<U>::type, float>::value &&
    !std::is_same<typename std::decay<U>::type, double>::value &&
    !std::is_same<typename std::decay<U>::type, const char*>::value &&
    !std::is_same<typename std::decay<U>::type, char*>::value &&
    !std::is_same<typename std::decay<U>::type, String>::value> {};

  // Auto-Stringification Macro Helpers
  template <typename T>
  typename std::enable_if<!std::is_convertible<T, const char*>::value>::type
  addVariableAuto(const char* autoName, T& var) {
    addVariable(_cleanKeyName(autoName), var, true, nullptr);
  }

  template <typename T, typename CB>
  typename std::enable_if<!std::is_convertible<T, const char*>::value && is_callable<CB>::value>::type
  addVariableAuto(const char* autoName, T& var, CB cb) {
    addVariable(_cleanKeyName(autoName), var, true, VariableCallback(cb));
  }

  template <typename T>
  typename std::enable_if<!std::is_convertible<T, const char*>::value>::type
  addVariableAuto(const char* autoName, T& var, bool writable) {
    addVariable(_cleanKeyName(autoName), var, writable, nullptr);
  }

  template <typename T, typename CB>
  typename std::enable_if<!std::is_convertible<T, const char*>::value && is_callable<CB>::value>::type
  addVariableAuto(const char* autoName, T& var, bool writable, CB cb) {
    addVariable(_cleanKeyName(autoName), var, writable, VariableCallback(cb));
  }

  template <typename T>
  typename std::enable_if<!std::is_convertible<T, const char*>::value>::type
  addVariableAuto(const char* autoName, T& var, bool writable, T minVal, T maxVal) {
    addVariable(_cleanKeyName(autoName), var, writable, minVal, maxVal, nullptr);
  }

  template <typename T, typename CB>
  typename std::enable_if<!std::is_convertible<T, const char*>::value && is_callable<CB>::value>::type
  addVariableAuto(const char* autoName, T& var, bool writable, T minVal, T maxVal, CB cb) {
    addVariable(_cleanKeyName(autoName), var, writable, minVal, maxVal, VariableCallback(cb));
  }

  template <typename T>
  void addVariableAuto(const char* autoName, const char* explicitName, T& var) {
    addVariable(String(explicitName), var, true, nullptr);
  }

  template <typename T, typename CB>
  typename std::enable_if<is_callable<CB>::value>::type
  addVariableAuto(const char* autoName, const char* explicitName, T& var, CB cb) {
    addVariable(String(explicitName), var, true, VariableCallback(cb));
  }

  template <typename T>
  void addVariableAuto(const char* autoName, const char* explicitName, T& var, bool writable) {
    addVariable(String(explicitName), var, writable, nullptr);
  }

  template <typename T, typename CB>
  typename std::enable_if<is_callable<CB>::value>::type
  addVariableAuto(const char* autoName, const char* explicitName, T& var, bool writable, CB cb) {
    addVariable(String(explicitName), var, writable, VariableCallback(cb));
  }

  template <typename T>
  void addVariableAuto(const char* autoName, const char* explicitName, T& var, bool writable, T minVal, T maxVal) {
    addVariable(String(explicitName), var, writable, minVal, maxVal, nullptr);
  }

  template <typename T, typename CB>
  typename std::enable_if<is_callable<CB>::value>::type
  addVariableAuto(const char* autoName, const char* explicitName, T& var, bool writable, T minVal, T maxVal, CB cb) {
    addVariable(String(explicitName), var, writable, minVal, maxVal, VariableCallback(cb));
  }

  void resetSettings();

private:
  WebServer _server;
  DNSServer _dnsServer;
  
  const char* _apSSID;
  const char* _apPass;
  bool _keepAPAlive;
  bool _inAPMode;
  
  String _authUser;
  String _authPass;
  bool _useAuth;

  ManagedVariable _vars[20];
  size_t _varCount;

  SavedCredential _savedNetworks[5];
  size_t _savedCount;

  bool isAuthorized();
  bool isCaptivePortalRequest();
  void setupRoutes();
  void handleCaptivePortal();
  void handleRoot();
  void handleScan();
  void handleConnect();
  void handleGetSaved();
  void handleDeleteSaved();
  void handleGetVars();
  void handleSetVar();
  void handleNotFound();
  
  void loadAllCredentials();
  void saveAllCredentials();
  void addCredential(const String& ssid, const String& pass);
  void deleteCredential(const String& ssid);
  
  void startAP();
  bool autoConnectSavedWiFi(uint32_t timeoutMs = 8000);
  bool connectWiFi(const String &ssid, const String &pass, uint32_t timeoutMs = 8000);
  String buildHTML();

  String _cleanKeyName(const char* raw) {
    String str = String(raw);
    int commaIdx = str.indexOf(',');
    if (commaIdx != -1) {
      str = str.substring(0, commaIdx);
    }
    str.trim();
    return str;
  }
};

#ifndef BONEZEGEI_WIFIMANAGER_CPP
#define addVariable(...) addVariableAuto(#__VA_ARGS__, __VA_ARGS__)
#endif

#endif
