#define BONEZEGEI_WIFIMANAGER_CPP
#include "Bonezegei_WifiManager.h"

const byte DNS_PORT = 53;

#if defined(ESP8266)
struct EEPROMSavedWiFi {
  char ssid[32];
  char pass[64];
};
#endif

Bonezegei_WifiManager::Bonezegei_WifiManager() 
  : _server(80), _varCount(0), _savedCount(0), _keepAPAlive(true), _inAPMode(false), _useAuth(false) {}

void Bonezegei_WifiManager::setAuth(const char* username, const char* password) {
  _authUser = username;
  _authPass = password;
  _useAuth = (_authUser.length() > 0 && _authPass.length() > 0);
}

bool Bonezegei_WifiManager::isAuthorized() {
  if (!_useAuth) return true;
  if (!_server.authenticate(_authUser.c_str(), _authPass.c_str())) {
    _server.requestAuthentication();
    return false;
  }
  return true;
}

void Bonezegei_WifiManager::begin(const char* apSSID, const char* apPassword, bool keepAPAlive) {
  _apSSID = apSSID;
  _apPass = apPassword;
  _keepAPAlive = keepAPAlive;

#if defined(ESP8266)
  EEPROM.begin(512);
#endif

  loadAllCredentials();

  startAP();

  if (_savedCount > 0 && autoConnectSavedWiFi()) {
    Serial.println("\n[WifiManager] Connected to Wi-Fi Network!");
    Serial.print("[WifiManager] Station IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WifiManager] mDNS URL: http://bonezegei.local\n");

    if (!_keepAPAlive) {
      WiFi.mode(WIFI_STA);
      _inAPMode = false;
    }
  } else {
    Serial.println("\n[WifiManager] AP Mode active (Not connected to external Wi-Fi).");
    Serial.print("[WifiManager] AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  MDNS.begin("bonezegei");

  setupRoutes();
  _server.begin();
}

void Bonezegei_WifiManager::process() {
  if (_inAPMode) {
    _dnsServer.processNextRequest();
  }
#if defined(ESP8266)
  MDNS.update();
#endif
  _server.handleClient();
}

void Bonezegei_WifiManager::addVariable(const String& name, bool& var, bool writable, VariableCallback cb) {
  if (_varCount < 20) {
    _vars[_varCount++] = {name, (void*)&var, VAR_BOOL, writable, 0, 1, cb};
  }
}

void Bonezegei_WifiManager::addVariable(const String& name, String& var, bool writable, VariableCallback cb) {
  if (_varCount < 20) {
    _vars[_varCount++] = {name, (void*)&var, VAR_STRING, writable, 0, 0, cb};
  }
}

void Bonezegei_WifiManager::addVariable(const String& name, int& var, bool writable, VariableCallback cb) {
  addVariable(name, var, writable, 0, 100, cb);
}

void Bonezegei_WifiManager::addVariable(const String& name, int& var, bool writable, int minVal, int maxVal, VariableCallback cb) {
  if (_varCount < 20) {
    _vars[_varCount++] = {name, (void*)&var, VAR_INT, writable, (float)minVal, (float)maxVal, cb};
  }
}

void Bonezegei_WifiManager::addVariable(const String& name, float& var, bool writable, VariableCallback cb) {
  addVariable(name, var, writable, 0.0f, 100.0f, cb);
}

void Bonezegei_WifiManager::addVariable(const String& name, float& var, bool writable, float minVal, float maxVal, VariableCallback cb) {
  if (_varCount < 20) {
    _vars[_varCount++] = {name, (void*)&var, VAR_FLOAT, writable, minVal, maxVal, cb};
  }
}

void Bonezegei_WifiManager::startAP() {
  _inAPMode = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(_apSSID, _apPass);
  _dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}

bool Bonezegei_WifiManager::autoConnectSavedWiFi(uint32_t timeoutMs) {
  int n = WiFi.scanNetworks();

  for (int i = 0; i < n; ++i) {
    String scannedSSID = WiFi.SSID(i);
    for (size_t j = 0; j < _savedCount; ++j) {
      if (_savedNetworks[j].ssid == scannedSSID) {
        if (connectWiFi(_savedNetworks[j].ssid, _savedNetworks[j].pass, timeoutMs)) {
          return true;
        }
      }
    }
  }

  for (size_t j = 0; j < _savedCount; ++j) {
    if (connectWiFi(_savedNetworks[j].ssid, _savedNetworks[j].pass, timeoutMs)) {
      return true;
    }
  }

  return false;
}

bool Bonezegei_WifiManager::connectWiFi(const String &ssid, const String &pass, uint32_t timeoutMs) {
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

void Bonezegei_WifiManager::loadAllCredentials() {
  _savedCount = 0;
#if defined(ESP32)
  Preferences prefs;
  prefs.begin("wifi_store", true);
  _savedCount = prefs.getUInt("count", 0);
  if (_savedCount > 5) _savedCount = 5;
  for (size_t i = 0; i < _savedCount; ++i) {
    _savedNetworks[i].ssid = prefs.getString(("s_" + String(i)).c_str(), "");
    _savedNetworks[i].pass = prefs.getString(("p_" + String(i)).c_str(), "");
  }
  prefs.end();
#elif defined(ESP8266)
  uint8_t count = 0;
  EEPROM.get(0, count);
  if (count > 5) count = 0;
  _savedCount = count;
  int addr = 1;
  for (size_t i = 0; i < _savedCount; ++i) {
    EEPROMSavedWiFi item;
    EEPROM.get(addr, item);
    _savedNetworks[i].ssid = String(item.ssid);
    _savedNetworks[i].pass = String(item.pass);
    addr += sizeof(EEPROMSavedWiFi);
  }
#endif
}

void Bonezegei_WifiManager::saveAllCredentials() {
#if defined(ESP32)
  Preferences prefs;
  prefs.begin("wifi_store", false);
  prefs.putUInt("count", _savedCount);
  for (size_t i = 0; i < 5; ++i) {
    if (i < _savedCount) {
      prefs.putString(("s_" + String(i)).c_str(), _savedNetworks[i].ssid);
      prefs.putString(("p_" + String(i)).c_str(), _savedNetworks[i].pass);
    } else {
      prefs.remove(("s_" + String(i)).c_str());
      prefs.remove(("p_" + String(i)).c_str());
    }
  }
  prefs.end();
#elif defined(ESP8266)
  EEPROM.put(0, (uint8_t)_savedCount);
  int addr = 1;
  for (size_t i = 0; i < 5; ++i) {
    EEPROMSavedWiFi item = {0};
    if (i < _savedCount) {
      _savedNetworks[i].ssid.toCharArray(item.ssid, 32);
      _savedNetworks[i].pass.toCharArray(item.pass, 64);
    }
    EEPROM.put(addr, item);
    addr += sizeof(EEPROMSavedWiFi);
  }
  EEPROM.commit();
#endif
}

void Bonezegei_WifiManager::addCredential(const String& ssid, const String& pass) {
  for (size_t i = 0; i < _savedCount; ++i) {
    if (_savedNetworks[i].ssid == ssid) {
      _savedNetworks[i].pass = pass;
      saveAllCredentials();
      return;
    }
  }

  if (_savedCount < 5) {
    _savedNetworks[_savedCount++] = {ssid, pass};
  } else {
    for (size_t i = 0; i < 4; ++i) {
      _savedNetworks[i] = _savedNetworks[i + 1];
    }
    _savedNetworks[4] = {ssid, pass};
  }
  saveAllCredentials();
}

void Bonezegei_WifiManager::deleteCredential(const String& ssid) {
  for (size_t i = 0; i < _savedCount; ++i) {
    if (_savedNetworks[i].ssid == ssid) {
      for (size_t j = i; j < _savedCount - 1; ++j) {
        _savedNetworks[j] = _savedNetworks[j + 1];
      }
      _savedCount--;
      saveAllCredentials();
      return;
    }
  }
}

void Bonezegei_WifiManager::resetSettings() {
  _savedCount = 0;
  saveAllCredentials();
  WiFi.disconnect(true);
  ESP.restart();
}

void Bonezegei_WifiManager::setupRoutes() {
  auto servePortal = [this]() { handleRoot(); };

  _server.on("/", servePortal);
  _server.on("/generate_204", servePortal);
  _server.on("/gen_204", servePortal);
  _server.on("/fwlink", servePortal);
  _server.on("/hotspot-detect.html", servePortal);
  _server.on("/canonical.html", servePortal);
  _server.on("/connecttest.txt", servePortal);
  _server.on("/api/wifi/scan", [this]() { handleScan(); });
  _server.on("/api/wifi/saved", [this]() { handleGetSaved(); });
  _server.on("/api/wifi/delete", HTTP_POST, [this]() { handleDeleteSaved(); });
  _server.on("/api/wifi/connect", HTTP_POST, [this]() { handleConnect(); });
  _server.on("/api/variables", [this]() { handleGetVars(); });
  _server.on("/api/control", [this]() { handleSetVar(); });
  _server.onNotFound([this]() { handleNotFound(); });
}

void Bonezegei_WifiManager::handleRoot() {
  if (!isAuthorized()) return;
  _server.send(200, "text/html", buildHTML());
}

void Bonezegei_WifiManager::handleNotFound() {
  if (_inAPMode) {
    handleRoot();
  } else {
    _server.send(404, "text/plain", "Not Found");
  }
}

void Bonezegei_WifiManager::handleScan() {
  if (!isAuthorized()) return;
  int n = WiFi.scanNetworks();
  String json = "{\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",\"networks\":[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]}";
  _server.send(200, "application/json", json);
}

void Bonezegei_WifiManager::handleGetSaved() {
  if (!isAuthorized()) return;
  String json = "[";
  for (size_t i = 0; i < _savedCount; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + _savedNetworks[i].ssid + "\"}";
  }
  json += "]";
  _server.send(200, "application/json", json);
}

void Bonezegei_WifiManager::handleDeleteSaved() {
  if (!isAuthorized()) return;
  if (_server.hasArg("ssid")) {
    String ssid = _server.arg("ssid");
    deleteCredential(ssid);
    _server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    _server.send(400, "text/plain", "Missing ssid arg");
  }
}

void Bonezegei_WifiManager::handleConnect() {
  if (!isAuthorized()) return;
  String ssid = _server.arg("ssid");
  String pass = _server.arg("pass");
  addCredential(ssid, pass);
  _server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Saved network! Rebooting to connect...\"}");
  delay(1000);
  ESP.restart();
}

void Bonezegei_WifiManager::handleGetVars() {
  if (!isAuthorized()) return;
  String json = "[";
  for (size_t i = 0; i < _varCount; ++i) {
    if (i > 0) json += ",";
    json += "{\"label\":\"" + _vars[i].name + "\",";
    json += "\"writable\":" + String(_vars[i].isWritable ? "true" : "false") + ",";
    json += "\"min\":" + String(_vars[i].minVal) + ",\"max\":" + String(_vars[i].maxVal) + ",";
    
    switch (_vars[i].type) {
      case VAR_INT:
        json += "\"type\":\"int\",\"value\":" + String(*(int*)_vars[i].ptr);
        break;
      case VAR_FLOAT:
        json += "\"type\":\"float\",\"value\":" + String(*(float*)_vars[i].ptr);
        break;
      case VAR_BOOL:
        json += "\"type\":\"bool\",\"value\":" + String(*(bool*)_vars[i].ptr ? "true" : "false");
        break;
      case VAR_STRING:
        json += "\"type\":\"string\",\"value\":\"" + *(String*)_vars[i].ptr + "\"";
        break;
    }
    json += "}";
  }
  json += "]";
  _server.send(200, "application/json", json);
}

void Bonezegei_WifiManager::handleSetVar() {
  if (!isAuthorized()) return;
  if (!_server.hasArg("id") || !_server.hasArg("val")) {
    _server.send(400, "text/plain", "Missing args");
    return;
  }
  int id = _server.arg("id").toInt();
  String val = _server.arg("val");

  if (id >= 0 && (size_t)id < _varCount && _vars[id].isWritable) {
    switch (_vars[id].type) {
      case VAR_INT: *(int*)_vars[id].ptr = val.toInt(); break;
      case VAR_FLOAT: *(float*)_vars[id].ptr = val.toFloat(); break;
      case VAR_BOOL: *(bool*)_vars[id].ptr = (val == "true" || val == "1"); break;
      case VAR_STRING: *(String*)_vars[id].ptr = val; break;
    }

    if (_vars[id].callback) {
      _vars[id].callback();
    }

    _server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    _server.send(403, "text/plain", "Read-only or invalid ID");
  }
}

String Bonezegei_WifiManager::buildHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Bonezegei WifiManager</title>
<style>
  body { font-family: Arial, sans-serif; background: #121212; color: #e0e0e0; margin: 0; padding: 0; }
  .navbar { background: #1f1f1f; display: flex; justify-content: space-between; align-items: center; padding: 12px 20px; border-bottom: 1px solid #333; }
  .navbar h2 { margin: 0; font-size: 1.1rem; color: #00e676; }
  .nav-btn { background: #2a2a2a; border: 1px solid #444; color: #aaa; font-size: 0.85rem; font-weight: bold; cursor: pointer; padding: 6px 12px; border-radius: 4px; margin-left: 5px; }
  .nav-btn.active { background: #00e676; color: #000; border-color: #00e676; }
  .container { padding: 20px; max-width: 500px; margin: 0 auto; }
  .tab-content { display: none; }
  .tab-content.active { display: block; }
  .card { background: #1e1e1e; padding: 18px; border-radius: 8px; margin-bottom: 15px; border: 1px solid #2a2a2a; }
  .var-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px; }
  .var-label { font-weight: bold; font-size: 0.95rem; }
  .var-value { color: #00e676; font-family: monospace; font-size: 1.1rem; }
  input[type=text], input[type=password] { width: 100%; padding: 10px; margin: 8px 0; box-sizing: border-box; background: #2a2a2a; color: #fff; border: 1px solid #444; border-radius: 4px; }
  .pass-container { position: relative; width: 100%; }
  .toggle-pass { position: absolute; right: 10px; top: 50%; transform: translateY(-50%); cursor: pointer; color: #00e676; font-size: 0.8rem; font-weight: bold; user-select: none; }
  button.btn { background: #00e676; color: #000; font-weight: bold; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; width: 100%; margin-top: 10px; }
  .wifi-item { background: #2a2a2a; padding: 10px; margin-bottom: 8px; border-radius: 4px; cursor: pointer; display: flex; justify-content: space-between; align-items: center; }
  .btn-forget { background: #ff5252; color: #fff; border: none; padding: 4px 10px; border-radius: 4px; cursor: pointer; font-size: 0.8rem; }
  .switch { position: relative; display: inline-block; width: 46px; height: 22px; }
  .switch input { opacity: 0; width: 0; height: 0; }
  .slider-toggle { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #444; transition: .3s; border-radius: 22px; }
  .slider-toggle:before { position: absolute; content: ""; height: 16px; width: 16px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
  input:checked + .slider-toggle { background-color: #00e676; }
  input:checked + .slider-toggle:before { transform: translateX(24px); }
</style>
</head>
<body>
  <div class="navbar">
    <h2>Bonezegei WifiManager</h2>
    <div>
      <button class="nav-btn active" id="btn-dash" onclick="switchTab('dash')">Variables</button>
      <button class="nav-btn" id="btn-wifi" onclick="switchTab('wifi')">Wi-Fi</button>
    </div>
  </div>
  <div class="container">
    <div id="tab-dash" class="tab-content active">
      <div class="card">
        <h3>Realtime Dashboard</h3>
        <div id="var-list">Loading variables...</div>
      </div>
    </div>
    <div id="tab-wifi" class="tab-content">
      <div class="card">
        <h3>Saved Wi-Fi Networks</h3>
        <div id="saved-list">Loading saved networks...</div>
      </div>
      <div class="card">
        <h3>Connect to Network</h3>
        <p>Status: <span id="wifi-status">Checking...</span></p>
        <button class="btn" style="background:#333; color:#fff;" onclick="scanWifi()">Scan Networks</button>
        <div id="wifi-list" style="margin-top:12px;"></div>
        <form onsubmit="connectWifi(event)">
          <input type="text" id="ssid" placeholder="SSID" required>
          <div class="pass-container">
            <input type="password" id="pass" placeholder="Password">
            <span class="toggle-pass" id="eye-btn" onclick="togglePass()">Show</span>
          </div>
          <button type="submit" class="btn">Save & Connect</button>
        </form>
      </div>
    </div>
  </div>
  <script>
    function switchTab(tab) {
      document.querySelectorAll('.tab-content').forEach(e => e.classList.remove('active'));
      document.querySelectorAll('.nav-btn').forEach(e => e.classList.remove('active'));
      document.getElementById('tab-' + tab).classList.add('active');
      document.getElementById('btn-' + (tab === 'dash' ? 'dash' : 'wifi')).classList.add('active');
      if (tab === 'wifi') {
        scanWifi();
        fetchSavedWifi();
      }
    }

    function togglePass() {
      let p = document.getElementById('pass');
      let eye = document.getElementById('eye-btn');
      if (p.type === 'password') {
        p.type = 'text';
        eye.innerText = 'Hide';
      } else {
        p.type = 'password';
        eye.innerText = 'Show';
      }
    }

    async function fetchVars() {
      try {
        let res = await fetch('/api/variables');
        let data = await res.json();
        let html = '';
        data.forEach((v, idx) => {
          html += `<div class="var-row"><span class="var-label">${v.label}</span>`;
          if (v.writable) {
            if (v.type === 'bool') {
              html += `<label class="switch"><input type="checkbox" ${v.value ? 'checked' : ''} onchange="setVar(${idx}, this.checked)"><span class="slider-toggle"></span></label>`;
            } else if (v.type === 'int' || v.type === 'float') {
              html += `<div style="display:flex; align-items:center; gap:8px;"><input type="range" min="${v.min}" max="${v.max}" step="${v.type==='float'?0.1:1}" value="${v.value}" onchange="setVar(${idx}, this.value)"><span class="var-value">${v.value}</span></div>`;
            } else {
              html += `<input type="text" value="${v.value}" onchange="setVar(${idx}, this.value)" style="width:120px;">`;
            }
          } else {
            html += `<span class="var-value">${v.value}</span>`;
          }
          html += `</div>`;
        });
        document.getElementById('var-list').innerHTML = html;
      } catch(e) {}
    }

    async function setVar(index, val) {
      await fetch(`/api/control?id=${index}&val=${encodeURIComponent(val)}`);
      fetchVars();
    }

    async function scanWifi() {
      document.getElementById('wifi-list').innerHTML = 'Scanning...';
      try {
        let res = await fetch('/api/wifi/scan');
        let data = await res.json();
        let html = '';
        data.networks.forEach(net => {
          html += `<div class="wifi-item" onclick="document.getElementById('ssid').value='${net.ssid}'"><span>${net.ssid}</span><span>${net.rssi} dBm</span></div>`;
        });
        document.getElementById('wifi-list').innerHTML = html || 'No networks found';
        document.getElementById('wifi-status').innerText = data.connected ? `Connected (${data.ip})` : 'AP Mode';
      } catch(e) { document.getElementById('wifi-list').innerHTML = 'Scan failed'; }
    }

    async function fetchSavedWifi() {
      try {
        let res = await fetch('/api/wifi/saved');
        let data = await res.json();
        let html = '';
        if (data.length === 0) {
          html = '<p style="color:#777; font-size:0.9rem;">No saved networks</p>';
        } else {
          data.forEach(item => {
            html += `<div class="wifi-item" style="cursor:default;">
                      <span>${item.ssid}</span>
                      <button class="btn-forget" onclick="deleteSavedWifi('${item.ssid}')">Forget</button>
                    </div>`;
          });
        }
        document.getElementById('saved-list').innerHTML = html;
      } catch(e) {}
    }

    async function deleteSavedWifi(ssid) {
      if(!confirm(`Forget network "${ssid}"?`)) return;
      await fetch('/api/wifi/delete', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `ssid=${encodeURIComponent(ssid)}`
      });
      fetchSavedWifi();
    }

    async function connectWifi(e) {
      e.preventDefault();
      let ssid = document.getElementById('ssid').value;
      let pass = document.getElementById('pass').value;
      let res = await fetch('/api/wifi/connect', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`
      });
      let data = await res.json();
      alert(data.message);
    }

    setInterval(fetchVars, 1000);
    fetchVars();
  </script>
</body>
</html>
)rawliteral";
}