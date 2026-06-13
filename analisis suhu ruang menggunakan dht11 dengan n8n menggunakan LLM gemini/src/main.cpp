#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "DHT.h"

#define DHTPIN 4
#define ledpin 2
#define DHTTYPE DHT11

// DHT Sensor
DHT dht(DHTPIN, DHTTYPE);
unsigned long lastSend = 0;
unsigned long lastReconnectAttempt = 0;

// Web Server
WebServer server(80);

Preferences preferences;


char wifi_ssid[64] = "Oort";
char wifi_password[64] = "hendryk1723";
char mqtt_server[128] = "broker.emqx.io";
char mqtt_topic[128] = "selviashintavirdiani/dht-data";

//konfigurasi ap
const char *ap_ssid = "DHT11-SELVIA-SHINTA-AP";
const char *ap_password = "12345678";

WiFiClient espClient;
PubSubClient client(espClient);


bool shouldReconnectWiFi = false;
int staAttemptCount = 0;
const int MAX_STA_ATTEMPTS = 4;
bool staBlocked = false;
void callback(char *topic, byte *payload, unsigned int length)
{
}

void loadConfig()
{
  preferences.begin("dht-config", true);
  
  preferences.getString("wifi_ssid", wifi_ssid, sizeof(wifi_ssid));
  preferences.getString("wifi_pass", wifi_password, sizeof(wifi_password));
  preferences.getString("mqtt_server", mqtt_server, sizeof(mqtt_server));
  preferences.getString("mqtt_topic", mqtt_topic, sizeof(mqtt_topic));
  
  preferences.end();
  
  // jika ssid kosong pake default
  if (strlen(wifi_ssid) == 0) {
    strcpy(wifi_ssid, "Oort");
  }
  if (strlen(wifi_password) == 0) {
    strcpy(wifi_password, "hendryk1723");
  }
  if (strlen(mqtt_server) == 0) {
    strcpy(mqtt_server, "broker.emqx.io");
  }
  if (strlen(mqtt_topic) == 0) {
    strcpy(mqtt_topic, "selviashintavirdiani/dht-data");
  }
  
  Serial.println("load konfigurasi dari preferences:");
  Serial.print("WiFi SSID: "); Serial.println(wifi_ssid);
  Serial.print("MQTT Server: "); Serial.println(mqtt_server);
  Serial.print("MQTT Topic: "); Serial.println(mqtt_topic);
}


void saveConfig()
{
  preferences.begin("dht-config", false);
  
  preferences.putString("wifi_ssid", wifi_ssid);
  preferences.putString("wifi_pass", wifi_password);
  preferences.putString("mqtt_server", mqtt_server);
  preferences.putString("mqtt_topic", mqtt_topic);
  
  preferences.end();
  
  Serial.println("konfigurasi tersimpan ke preferences:");
}
void ledblink(int times, int delayTime)
{
  for (int i = 0; i < times; i++)
  {
    digitalWrite(ledpin, HIGH);
    delay(delayTime);
    digitalWrite(ledpin, LOW);
    delay(delayTime);
  }
}
void setupWiFiAP()
{
  Serial.println("\nStarting WiFi AP+STA mode...");
  
  // Start in AP+STA mode
  WiFi.mode(WIFI_AP_STA);
  
  // Setup AP
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apIP);
  Serial.print("AP SSID: ");
  Serial.println(ap_ssid);
  Serial.println("Connect to this AP to configure WiFi settings");
  ledblink(3, 400);

}

// ================= CONNECT WIFI STA =================
void setupWiFiSTA()
{
  Serial.print("\nConnecting to WiFi STA: ");
  Serial.println(wifi_ssid);
  
  WiFi.begin(wifi_ssid, wifi_password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 16)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi Connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    staAttemptCount = 0;
    staBlocked = false;
    ledblink(1,500);
  }
  else
  {
    staAttemptCount++;
    Serial.println("\nFailed to connect to WiFi (attempt " + String(staAttemptCount) + ")");
    if (staAttemptCount >= MAX_STA_ATTEMPTS)
    {
      staBlocked = true;
      Serial.println("Reached maximum STA attempts. Leaving AP active for reconfiguration.");
      Serial.println("Connect to the AP and submit new settings to retry.");
    }
    else
    {
      Serial.println("Will retry in next loop");
    }
  }
}

// ================= HTML PAGE FOR CONFIGURATION =================
String getConfigHTML()
{
  String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DHT11 Configuration</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 600px;
            margin: 50px auto;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            background-color: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
            margin-bottom: 30px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #555;
            font-weight: bold;
        }
        input[type="text"],
        input[type="password"] {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
            box-sizing: border-box;
            font-size: 14px;
        }
        input[type="text"]:focus,
        input[type="password"]:focus {
            outline: none;
            border-color: #4CAF50;
            box-shadow: 0 0 5px rgba(76, 175, 80, 0.3);
        }
        button {
            background-color: #4CAF50;
            color: white;
            padding: 12px 30px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
            font-weight: bold;
            width: 100%;
            transition: background-color 0.3s;
        }
        button:hover {
            background-color: #45a049;
        }
        .info {
            background-color: #e3f2fd;
            padding: 15px;
            border-left: 4px solid #2196F3;
            margin-bottom: 20px;
            border-radius: 4px;
            color: #1565c0;
        }
        .status {
            margin-top: 20px;
            padding: 10px;
            border-radius: 4px;
            text-align: center;
        }
        .status.success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .status.error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>DHT11 WiFi Configuration</h1>
        
        <div class="info">
            <strong>ℹ️ Info:</strong> Configure your WiFi and MQTT settings below. After submitting, the device will reconnect with the new settings.
        </div>

        <form method="POST" action="/submit">
            <div class="form-group">
                <label for="ssid">WiFi SSID:</label>
                <input type="text" id="ssid" name="ssid" value=")HTML" + String(wifi_ssid) + R"HTML(" required>
            </div>

            <div class="form-group">
                <label for="password">WiFi Password:</label>
                <input type="password" id="password" name="password" value=")HTML" + String(wifi_password) + R"HTML(" required>
            </div>

            <div class="form-group">
                <label for="mqtt_server">MQTT Broker:</label>
                <input type="text" id="mqtt_server" name="mqtt_server" value=")HTML" + String(mqtt_server) + R"HTML(" required>
            </div>

            <div class="form-group">
                <label for="mqtt_topic">MQTT Topic (Publish):</label>
                <input type="text" id="mqtt_topic" name="mqtt_topic" value=")HTML" + String(mqtt_topic) + R"HTML(" required>
            </div>

            <button type="submit">Save & Reconnect</button>
        </form>

        <div style="margin-top: 20px; padding: 15px; background-color: #f9f9f9; border-radius: 4px; font-size: 12px; color: #666;">
            <strong>Current Configuration:</strong><br>
            WiFi SSID: )HTML" + String(wifi_ssid) + R"HTML(<br>
            MQTT Broker: )HTML" + String(mqtt_server) + R"HTML(<br>
            MQTT Topic: )HTML" + String(mqtt_topic) + R"HTML(
        </div>
    </div>
</body>
</html>
)HTML";
  
  return html;
}

// ================= WEB SERVER HANDLERS =================
void handleRoot()
{
  server.send(200, "text/html", getConfigHTML());
}

void handleSubmit()
{
  // Check if all parameters are present
  if (server.hasArg("ssid") && server.hasArg("password") && 
      server.hasArg("mqtt_server") && server.hasArg("mqtt_topic"))
  {
    // Get values from form
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");
    String new_mqtt_server = server.arg("mqtt_server");
    String new_mqtt_topic = server.arg("mqtt_topic");
    
    // Update configuration variables
    strncpy(wifi_ssid, new_ssid.c_str(), sizeof(wifi_ssid) - 1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';
    
    strncpy(wifi_password, new_password.c_str(), sizeof(wifi_password) - 1);
    wifi_password[sizeof(wifi_password) - 1] = '\0';
    
    strncpy(mqtt_server, new_mqtt_server.c_str(), sizeof(mqtt_server) - 1);
    mqtt_server[sizeof(mqtt_server) - 1] = '\0';
    
    strncpy(mqtt_topic, new_mqtt_topic.c_str(), sizeof(mqtt_topic) - 1);
    mqtt_topic[sizeof(mqtt_topic) - 1] = '\0';
    
    // Save to preferences
    saveConfig();
    
    // Set flag to reconnect WiFi
    shouldReconnectWiFi = true;
    // Reset STA attempt tracking so we try immediately after submit
    staAttemptCount = 0;
    staBlocked = false;
    
    // Send success response
    String response = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Configuration Saved</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
            background-color: #f5f5f5;
        }
        .container {
            text-align: center;
            background-color: white;
            padding: 40px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 {
            color: #4CAF50;
            margin: 0 0 20px 0;
        }
        p {
            color: #666;
            margin: 10px 0;
        }
        .info {
            background-color: #e3f2fd;
            padding: 15px;
            border-left: 4px solid #2196F3;
            margin: 20px 0;
            border-radius: 4px;
            color: #1565c0;
            font-size: 14px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>✓ konfigurasi tersimpan!</h1>
        <p>alat sudah menerima konfigurasi baru dan akan mencoba untuk terhubung ke WiFi dengan pengaturan tersebut.</p>
        <div class="info">
            <strong>New Settings Applied:</strong><br>
            WiFi SSID: )HTML" + String(wifi_ssid) + R"HTML(<br>
            MQTT Broker: )HTML" + String(mqtt_server) + R"HTML(<br>
            MQTT Topic: )HTML" + String(mqtt_topic) + R"HTML(
        </div>
        <p>You will be disconnected from the AP. Please connect to your WiFi network.</p>
        <p>The device will be available at: <strong>http://)HTML" + WiFi.localIP().toString() + R"HTML(</strong></p>
    </div>
</body>
</html>
)HTML";
    
    server.send(200, "text/html", response);
    
    Serial.println("\n========== NEW CONFIGURATION RECEIVED ==========");
    Serial.print("WiFi SSID: "); Serial.println(wifi_ssid);
    Serial.print("WiFi Password: "); Serial.println(wifi_password);
    Serial.print("MQTT Server: "); Serial.println(mqtt_server);
    Serial.print("MQTT Topic: "); Serial.println(mqtt_topic);
    Serial.println("Reconnecting WiFi in 2 seconds...");
    Serial.println("==============================================\n");
  }
  else
  {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleNotFound()
{
  server.send(404, "text/plain", "404: Not Found");
}

// ================= SETUP WEB SERVER =================
void setupWebServer()
{
  server.on("/", handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web Server started on port 80");
}

// ================= RECONNECT MQTT =================
void reconnectMQTT()
{
  if (millis() - lastReconnectAttempt > 2000)
  {
    lastReconnectAttempt = millis();

    Serial.print("Connecting MQTT to ");
    Serial.print(mqtt_server);
    Serial.print("...");

    String clientId = "ESP32-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str()))
    {
      Serial.println("Connected");
    }
    else
    {
      Serial.print("Failed, rc=");
      Serial.println(client.state());
    }
  }
}


void setup()
{
  Serial.begin(9600);
  pinMode(ledpin, OUTPUT);
  delay(1000);
  
  Serial.println("\n\n========== DHT11 WiFi+MQTT Configuration ==========");
  
  // Load configuration from preferences
  loadConfig();
  
  // Setup WiFi in AP+STA mode
  setupWiFiAP();
  setupWiFiSTA();
  
  // Setup Web Server
  setupWebServer();
  
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize MQTT client with loaded configuration
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.setBufferSize(512);
  
  Serial.println("====================================================\n");
}

// ================= LOOP =================
void loop()
{
  // Handle web server requests
  server.handleClient();
  
  // ===== CHECK IF WIFI SETTINGS WERE UPDATED =====
  if (shouldReconnectWiFi)
  {
    shouldReconnectWiFi = false;
    delay(2000);
    
    // Disconnect current WiFi STA
    WiFi.disconnect(false);
    delay(1000);
    
    // Ensure STA tracking cleared and try reconnecting once
    staAttemptCount = 0;
    staBlocked = false;
    setupWiFiSTA();
    
    // Reinitialize MQTT with new server
    client.setServer(mqtt_server, 1883);
  }
  
  // ===== WIFI STA CONNECTION =====
  if (WiFi.status() != WL_CONNECTED)
  {
    if (staBlocked)
    {
      Serial.println("WiFi STA blocked after multiple failed attempts; AP remains active for reconfiguration.");
    }
    else
    {
      Serial.println("WiFi STA disconnected, attempting reconnection...");
      setupWiFiSTA();
    }
  }
  
  // ===== MQTT CONNECTION =====
  if (!client.connected())
  {
    reconnectMQTT();
  }
  else
  {
    client.loop();
    
    // ===== PUBLISH MQTT DATA =====
    if (millis() - lastSend > 2000)
    {
      lastSend = millis();
      
      // Read DHT data
      float suhu = dht.readTemperature();
      float kelembapan = dht.readHumidity();
      
      // Check if readings are valid
      if (isnan(suhu) || isnan(kelembapan))
      {
        Serial.println("Failed to read from DHT sensor!");
      }
      else
      {
        // Create JSON document
        StaticJsonDocument<200> doc;
        doc["suhu"] = suhu;
        doc["kelembapan"] = kelembapan;
        
        // Serialize to string
        char buffer[256];
        serializeJson(doc, buffer);
        
        // Print to serial
        Serial.print("Suhu: ");
        Serial.print(suhu);
        Serial.print("°C  Kelembapan: ");
        Serial.println(kelembapan);
        
        // Publish to MQTT
        if (client.publish(mqtt_topic, buffer))
        {
          Serial.print("✓ Published to ");
          Serial.println(mqtt_topic);
        }
        else
        {
          Serial.println("✗ Failed to publish to MQTT");
        }
      }
    }
  }
}