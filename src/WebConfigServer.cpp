#include "WebConfigServer.h"

WebConfigServer::WebConfigServer() : server(nullptr), serverStarted(false) {
}

WebConfigServer::~WebConfigServer() {
    if (server) {
        delete server;
    }
}

void WebConfigServer::begin() {
    Serial.println("Initializing Web Configuration Server...");
    
    // Load configurations
    loadWiFiConfig();
    loadDeviceConfigs();
    
    // Start WiFi
    startWiFi();
    
    // Start web server
    startServer();
}

void WebConfigServer::startWiFi() {
    Serial.println("Starting WiFi...");
    
    if (wifiConfig.apMode) {
        // Access Point mode
        Serial.println("Starting in AP mode...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Victron-Config", wifiConfig.apPassword.c_str());
        
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
    } else {
        // Station mode
        Serial.println("Connecting to WiFi...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("\nWiFi connection failed, falling back to AP mode");
            wifiConfig.apMode = true;
            WiFi.mode(WIFI_AP);
            WiFi.softAP("Victron-Config", wifiConfig.apPassword.c_str());
            Serial.print("AP IP address: ");
            Serial.println(WiFi.softAPIP());
        }
    }
}

void WebConfigServer::startServer() {
    if (serverStarted) return;
    
    server = new AsyncWebServer(80);
    
    // Serve main page
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleRoot(request);
    });
    
    // API endpoints
    server->on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetDevices(request);
    });
    
    server->on("/api/devices", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleAddDevice(request);
    });
    
    server->on("/api/devices/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleUpdateDevice(request);
    });
    
    server->on("/api/devices/delete", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleDeleteDevice(request);
    });
    
    server->on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetWiFiConfig(request);
    });
    
    server->on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSetWiFiConfig(request);
    });
    
    server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleRestart(request);
    });
    
    server->begin();
    serverStarted = true;
    Serial.println("Web server started");
}

void WebConfigServer::handleRoot(AsyncWebServerRequest *request) {
    String html = generateIndexPage();
    request->send(200, "text/html", html);
}

void WebConfigServer::handleGetDevices(AsyncWebServerRequest *request) {
    String json = "[";
    
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (i > 0) json += ",";
        
        json += "{";
        json += "\"name\":\"" + deviceConfigs[i].name + "\",";
        json += "\"address\":\"" + deviceConfigs[i].address + "\",";
        json += "\"encryptionKey\":\"" + deviceConfigs[i].encryptionKey + "\",";
        json += "\"enabled\":" + String(deviceConfigs[i].enabled ? "true" : "false");
        json += "}";
    }
    
    json += "]";
    request->send(200, "application/json", json);
}

void WebConfigServer::handleAddDevice(AsyncWebServerRequest *request) {
    if (request->hasParam("name", true) && request->hasParam("address", true)) {
        String name = request->getParam("name", true)->value();
        String address = request->getParam("address", true)->value();
        String key = request->hasParam("encryptionKey", true) ? 
                     request->getParam("encryptionKey", true)->value() : "";
        
        DeviceConfig config(name, address, key, true);
        addDeviceConfig(config);
        
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    }
}

void WebConfigServer::handleUpdateDevice(AsyncWebServerRequest *request) {
    if (request->hasParam("address", true) && request->hasParam("name", true)) {
        String address = request->getParam("address", true)->value();
        String name = request->getParam("name", true)->value();
        String key = request->hasParam("encryptionKey", true) ? 
                     request->getParam("encryptionKey", true)->value() : "";
        bool enabled = request->hasParam("enabled", true) ? 
                       request->getParam("enabled", true)->value() == "true" : true;
        
        DeviceConfig config(name, address, key, enabled);
        updateDeviceConfig(address, config);
        
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    }
}

void WebConfigServer::handleDeleteDevice(AsyncWebServerRequest *request) {
    if (request->hasParam("address", true)) {
        String address = request->getParam("address", true)->value();
        removeDeviceConfig(address);
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing address\"}");
    }
}

void WebConfigServer::handleGetWiFiConfig(AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"ssid\":\"" + wifiConfig.ssid + "\",";
    json += "\"apMode\":" + String(wifiConfig.apMode ? "true" : "false") + ",";
    json += "\"apPassword\":\"" + wifiConfig.apPassword + "\"";
    json += "}";
    
    request->send(200, "application/json", json);
}

void WebConfigServer::handleSetWiFiConfig(AsyncWebServerRequest *request) {
    bool changed = false;
    
    if (request->hasParam("ssid", true)) {
        wifiConfig.ssid = request->getParam("ssid", true)->value();
        changed = true;
    }
    
    if (request->hasParam("password", true)) {
        wifiConfig.password = request->getParam("password", true)->value();
        changed = true;
    }
    
    if (request->hasParam("apMode", true)) {
        wifiConfig.apMode = request->getParam("apMode", true)->value() == "true";
        changed = true;
    }
    
    if (request->hasParam("apPassword", true)) {
        wifiConfig.apPassword = request->getParam("apPassword", true)->value();
        changed = true;
    }
    
    if (changed) {
        saveWiFiConfig();
        request->send(200, "application/json", 
            "{\"success\":true,\"message\":\"Restart required for changes to take effect\"}");
    } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"No parameters provided\"}");
    }
}

void WebConfigServer::handleRestart(AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting...\"}");
    // Delay is intentional here to allow the HTTP response to complete
    // before restart. This is acceptable for a restart endpoint.
    delay(1000);
    ESP.restart();
}

String WebConfigServer::generateIndexPage() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Victron BLE Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: Arial, sans-serif; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container { 
            max-width: 800px; 
            margin: 0 auto; 
            background: white;
            border-radius: 10px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }
        .header h1 { font-size: 28px; margin-bottom: 10px; }
        .header p { opacity: 0.9; }
        .content { padding: 30px; }
        .section { margin-bottom: 30px; }
        .section h2 { 
            color: #667eea; 
            margin-bottom: 15px; 
            font-size: 20px;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        .device-list { 
            border: 1px solid #ddd; 
            border-radius: 5px; 
            overflow: hidden;
        }
        .device-item {
            padding: 15px;
            border-bottom: 1px solid #eee;
            display: flex;
            justify-content: space-between;
            align-items: center;
            transition: background 0.2s;
        }
        .device-item:hover { background: #f9f9f9; }
        .device-item:last-child { border-bottom: none; }
        .device-info { flex: 1; }
        .device-name { font-weight: bold; color: #333; margin-bottom: 5px; }
        .device-address { color: #666; font-size: 14px; font-family: monospace; }
        .device-key { color: #999; font-size: 12px; margin-top: 5px; }
        .device-actions button {
            margin-left: 10px;
            padding: 8px 15px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            transition: all 0.2s;
        }
        .btn-edit { background: #667eea; color: white; }
        .btn-edit:hover { background: #5568d3; }
        .btn-delete { background: #e74c3c; color: white; }
        .btn-delete:hover { background: #c0392b; }
        .btn-primary {
            background: #667eea;
            color: white;
            padding: 12px 25px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
            transition: all 0.2s;
        }
        .btn-primary:hover { background: #5568d3; }
        .form-group {
            margin-bottom: 20px;
        }
        .form-group label {
            display: block;
            margin-bottom: 5px;
            color: #333;
            font-weight: bold;
        }
        .form-group input, .form-group select {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
        }
        .form-group small {
            display: block;
            margin-top: 5px;
            color: #666;
            font-size: 12px;
        }
        .modal {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0,0,0,0.5);
            z-index: 1000;
        }
        .modal.active { display: flex; align-items: center; justify-content: center; }
        .modal-content {
            background: white;
            padding: 30px;
            border-radius: 10px;
            max-width: 500px;
            width: 90%;
            max-height: 90vh;
            overflow-y: auto;
        }
        .modal-header {
            margin-bottom: 20px;
            padding-bottom: 15px;
            border-bottom: 2px solid #667eea;
        }
        .modal-header h3 {
            color: #667eea;
            font-size: 22px;
        }
        .modal-buttons {
            display: flex;
            justify-content: flex-end;
            gap: 10px;
            margin-top: 20px;
        }
        .btn-secondary {
            background: #95a5a6;
            color: white;
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
        }
        .btn-secondary:hover { background: #7f8c8d; }
        .info-box {
            background: #e8f4f8;
            border-left: 4px solid #667eea;
            padding: 15px;
            margin-bottom: 20px;
            border-radius: 5px;
        }
        .info-box strong { color: #667eea; }
        .empty-state {
            text-align: center;
            padding: 40px 20px;
            color: #999;
        }
        .empty-state svg {
            width: 80px;
            height: 80px;
            margin-bottom: 20px;
            opacity: 0.5;
        }
        @media (max-width: 600px) {
            .device-item { flex-direction: column; align-items: flex-start; }
            .device-actions { margin-top: 10px; width: 100%; }
            .device-actions button { margin: 5px 5px 0 0; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔧 Victron BLE Configuration</h1>
            <p>Configure your Victron devices and encryption keys</p>
        </div>
        
        <div class="content">
            <div class="section">
                <h2>📱 Configured Devices</h2>
                <div class="info-box">
                    <strong>Note:</strong> Add your Victron devices with their BLE MAC addresses. 
                    If your device uses encryption, enter the 32-character encryption key from the VictronConnect app.
                </div>
                <div id="deviceList" class="device-list">
                    <div class="empty-state">
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                            <rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect>
                            <line x1="9" y1="9" x2="15" y2="9"></line>
                            <line x1="9" y1="15" x2="15" y2="15"></line>
                        </svg>
                        <p>No devices configured yet</p>
                    </div>
                </div>
                <button class="btn-primary" onclick="openAddDeviceModal()" style="margin-top: 15px;">
                    ➕ Add Device
                </button>
            </div>
            
            <div class="section">
                <h2>📡 WiFi Configuration</h2>
                <div id="wifiStatus" class="info-box">
                    <strong>Current Mode:</strong> <span id="currentMode">Loading...</span><br>
                    <strong>IP Address:</strong> <span id="ipAddress">Loading...</span>
                </div>
                <button class="btn-primary" onclick="openWiFiModal()">
                    ⚙️ Configure WiFi
                </button>
            </div>
        </div>
    </div>

    <!-- Add/Edit Device Modal -->
    <div id="deviceModal" class="modal">
        <div class="modal-content">
            <div class="modal-header">
                <h3 id="modalTitle">Add Device</h3>
            </div>
            <form id="deviceForm">
                <div class="form-group">
                    <label>Device Name</label>
                    <input type="text" id="deviceName" placeholder="e.g., SmartShunt 500A" required>
                    <small>Friendly name for the device</small>
                </div>
                <div class="form-group">
                    <label>BLE MAC Address</label>
                    <input type="text" id="deviceAddress" placeholder="e.g., AA:BB:CC:DD:EE:FF" required pattern="[A-Fa-f0-9:]{17}">
                    <small>MAC address from VictronConnect app</small>
                </div>
                <div class="form-group">
                    <label>Encryption Key (Optional)</label>
                    <input type="text" id="deviceKey" placeholder="e.g., 0123456789ABCDEF0123456789ABCDEF" pattern="[A-Fa-f0-9]{32}">
                    <small>32-character hex key (leave empty for instant readout mode)</small>
                </div>
                <div class="modal-buttons">
                    <button type="button" class="btn-secondary" onclick="closeDeviceModal()">Cancel</button>
                    <button type="submit" class="btn-primary">Save</button>
                </div>
            </form>
        </div>
    </div>

    <!-- WiFi Configuration Modal -->
    <div id="wifiModal" class="modal">
        <div class="modal-content">
            <div class="modal-header">
                <h3>WiFi Configuration</h3>
            </div>
            <form id="wifiForm">
                <div class="form-group">
                    <label>Mode</label>
                    <select id="wifiMode" onchange="toggleWiFiMode()">
                        <option value="ap">Access Point (AP)</option>
                        <option value="sta">Station (Connect to WiFi)</option>
                    </select>
                </div>
                <div id="staFields" style="display: none;">
                    <div class="form-group">
                        <label>WiFi SSID</label>
                        <input type="text" id="wifiSSID" placeholder="Your WiFi network name">
                    </div>
                    <div class="form-group">
                        <label>WiFi Password</label>
                        <input type="password" id="wifiPassword" placeholder="WiFi password">
                    </div>
                </div>
                <div id="apFields">
                    <div class="form-group">
                        <label>AP Password</label>
                        <input type="password" id="apPassword" placeholder="Password for AP mode">
                        <small>Minimum 8 characters</small>
                    </div>
                </div>
                <div class="info-box">
                    <strong>⚠️ Warning:</strong> Changing WiFi settings requires a restart to take effect.
                </div>
                <div class="modal-buttons">
                    <button type="button" class="btn-secondary" onclick="closeWiFiModal()">Cancel</button>
                    <button type="submit" class="btn-primary">Save & Restart</button>
                </div>
            </form>
        </div>
    </div>

    <script>
        let editingAddress = null;

        // Load devices on page load
        window.onload = function() {
            loadDevices();
            loadWiFiStatus();
        };

        function loadDevices() {
            fetch('/api/devices')
                .then(r => r.json())
                .then(devices => {
                    const list = document.getElementById('deviceList');
                    if (devices.length === 0) {
                        list.innerHTML = '<div class="empty-state"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect><line x1="9" y1="9" x2="15" y2="9"></line><line x1="9" y1="15" x2="15" y2="15"></line></svg><p>No devices configured yet</p></div>';
                    } else {
                        list.innerHTML = devices.map(d => `
                            <div class="device-item">
                                <div class="device-info">
                                    <div class="device-name">${d.name}</div>
                                    <div class="device-address">${d.address}</div>
                                    ${d.encryptionKey ? '<div class="device-key">🔒 Encrypted</div>' : '<div class="device-key">🔓 Instant Readout</div>'}
                                </div>
                                <div class="device-actions">
                                    <button class="btn-edit" onclick='editDevice(${JSON.stringify(d)})'>Edit</button>
                                    <button class="btn-delete" onclick="deleteDevice('${d.address}')">Delete</button>
                                </div>
                            </div>
                        `).join('');
                    }
                });
        }

        function loadWiFiStatus() {
            fetch('/api/wifi')
                .then(r => r.json())
                .then(wifi => {
                    document.getElementById('currentMode').textContent = wifi.apMode ? 'Access Point' : 'Station';
                    document.getElementById('ipAddress').textContent = wifi.apMode ? 
                        'Connect to Victron-Config' : (wifi.ssid || 'Not configured');
                });
        }

        function openAddDeviceModal() {
            editingAddress = null;
            document.getElementById('modalTitle').textContent = 'Add Device';
            document.getElementById('deviceName').value = '';
            document.getElementById('deviceAddress').value = '';
            document.getElementById('deviceKey').value = '';
            document.getElementById('deviceModal').classList.add('active');
        }

        function editDevice(device) {
            editingAddress = device.address;
            document.getElementById('modalTitle').textContent = 'Edit Device';
            document.getElementById('deviceName').value = device.name;
            document.getElementById('deviceAddress').value = device.address;
            document.getElementById('deviceKey').value = device.encryptionKey || '';
            document.getElementById('deviceModal').classList.add('active');
        }

        function closeDeviceModal() {
            document.getElementById('deviceModal').classList.remove('active');
        }

        function deleteDevice(address) {
            if (confirm('Are you sure you want to delete this device?')) {
                const formData = new FormData();
                formData.append('address', address);
                
                fetch('/api/devices/delete', {
                    method: 'POST',
                    body: formData
                })
                .then(r => r.json())
                .then(result => {
                    if (result.success) {
                        loadDevices();
                    } else {
                        alert('Error: ' + result.error);
                    }
                });
            }
        }

        document.getElementById('deviceForm').onsubmit = function(e) {
            e.preventDefault();
            
            const formData = new FormData();
            formData.append('name', document.getElementById('deviceName').value);
            formData.append('address', document.getElementById('deviceAddress').value.toUpperCase());
            formData.append('encryptionKey', document.getElementById('deviceKey').value);
            
            const url = editingAddress ? '/api/devices/update' : '/api/devices';
            
            fetch(url, {
                method: 'POST',
                body: formData
            })
            .then(r => r.json())
            .then(result => {
                if (result.success) {
                    closeDeviceModal();
                    loadDevices();
                } else {
                    alert('Error: ' + result.error);
                }
            });
        };

        function openWiFiModal() {
            fetch('/api/wifi')
                .then(r => r.json())
                .then(wifi => {
                    document.getElementById('wifiMode').value = wifi.apMode ? 'ap' : 'sta';
                    document.getElementById('wifiSSID').value = wifi.ssid || '';
                    document.getElementById('wifiPassword').value = '';
                    document.getElementById('apPassword').value = wifi.apPassword || '';
                    toggleWiFiMode();
                    document.getElementById('wifiModal').classList.add('active');
                });
        }

        function closeWiFiModal() {
            document.getElementById('wifiModal').classList.remove('active');
        }

        function toggleWiFiMode() {
            const mode = document.getElementById('wifiMode').value;
            document.getElementById('staFields').style.display = mode === 'sta' ? 'block' : 'none';
            document.getElementById('apFields').style.display = mode === 'ap' ? 'block' : 'none';
        }

        document.getElementById('wifiForm').onsubmit = function(e) {
            e.preventDefault();
            
            const formData = new FormData();
            const mode = document.getElementById('wifiMode').value;
            formData.append('apMode', mode === 'ap' ? 'true' : 'false');
            
            if (mode === 'sta') {
                formData.append('ssid', document.getElementById('wifiSSID').value);
                const pwd = document.getElementById('wifiPassword').value;
                if (pwd) formData.append('password', pwd);
            } else {
                formData.append('apPassword', document.getElementById('apPassword').value);
            }
            
            fetch('/api/wifi', {
                method: 'POST',
                body: formData
            })
            .then(r => r.json())
            .then(result => {
                if (result.success) {
                    alert('Settings saved! Device will restart...');
                    fetch('/api/restart', { method: 'POST' });
                } else {
                    alert('Error: ' + result.error);
                }
            });
        };
    </script>
</body>
</html>
)";
    
    return html;
}

// Configuration persistence methods
void WebConfigServer::saveWiFiConfig() {
    preferences.begin("victron-wifi", false);
    preferences.putString("ssid", wifiConfig.ssid);
    preferences.putString("password", wifiConfig.password);
    preferences.putBool("apMode", wifiConfig.apMode);
    preferences.putString("apPassword", wifiConfig.apPassword);
    preferences.end();
    Serial.println("WiFi config saved");
}

void WebConfigServer::loadWiFiConfig() {
    preferences.begin("victron-wifi", true);
    wifiConfig.ssid = preferences.getString("ssid", "");
    wifiConfig.password = preferences.getString("password", "");
    wifiConfig.apMode = preferences.getBool("apMode", true);
    wifiConfig.apPassword = preferences.getString("apPassword", "victron123");
    preferences.end();
    Serial.println("WiFi config loaded");
}

void WebConfigServer::saveDeviceConfigs() {
    preferences.begin("victron-dev", false);
    preferences.putInt("count", deviceConfigs.size());
    
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        String prefix = "dev" + String(i) + "_";
        preferences.putString((prefix + "name").c_str(), deviceConfigs[i].name);
        preferences.putString((prefix + "addr").c_str(), deviceConfigs[i].address);
        preferences.putString((prefix + "key").c_str(), deviceConfigs[i].encryptionKey);
        preferences.putBool((prefix + "en").c_str(), deviceConfigs[i].enabled);
    }
    
    preferences.end();
    Serial.println("Device configs saved");
}

void WebConfigServer::loadDeviceConfigs() {
    preferences.begin("victron-dev", true);
    int count = preferences.getInt("count", 0);
    
    deviceConfigs.clear();
    for (int i = 0; i < count; i++) {
        String prefix = "dev" + String(i) + "_";
        DeviceConfig config;
        config.name = preferences.getString((prefix + "name").c_str(), "");
        config.address = preferences.getString((prefix + "addr").c_str(), "");
        config.encryptionKey = preferences.getString((prefix + "key").c_str(), "");
        config.enabled = preferences.getBool((prefix + "en").c_str(), true);
        
        if (!config.address.isEmpty()) {
            deviceConfigs.push_back(config);
        }
    }
    
    preferences.end();
    Serial.printf("Loaded %d device configs\n", deviceConfigs.size());
}

// Device configuration access methods
std::vector<DeviceConfig>& WebConfigServer::getDeviceConfigs() {
    return deviceConfigs;
}

DeviceConfig* WebConfigServer::getDeviceConfig(const String& address) {
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (deviceConfigs[i].address.equalsIgnoreCase(address)) {
            return &deviceConfigs[i];
        }
    }
    return nullptr;
}

void WebConfigServer::addDeviceConfig(const DeviceConfig& config) {
    // Check if device already exists
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (deviceConfigs[i].address.equalsIgnoreCase(config.address)) {
            // Update existing
            deviceConfigs[i] = config;
            saveDeviceConfigs();
            return;
        }
    }
    
    // Add new
    deviceConfigs.push_back(config);
    saveDeviceConfigs();
}

void WebConfigServer::updateDeviceConfig(const String& address, const DeviceConfig& config) {
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (deviceConfigs[i].address.equalsIgnoreCase(address)) {
            deviceConfigs[i] = config;
            saveDeviceConfigs();
            return;
        }
    }
}

void WebConfigServer::removeDeviceConfig(const String& address) {
    for (size_t i = 0; i < deviceConfigs.size(); i++) {
        if (deviceConfigs[i].address.equalsIgnoreCase(address)) {
            deviceConfigs.erase(deviceConfigs.begin() + i);
            saveDeviceConfigs();
            return;
        }
    }
}

// WiFi info methods
String WebConfigServer::getIPAddress() {
    if (wifiConfig.apMode) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

bool WebConfigServer::isWiFiConnected() {
    return wifiConfig.apMode || WiFi.status() == WL_CONNECTED;
}

bool WebConfigServer::isAPMode() {
    return wifiConfig.apMode;
}
