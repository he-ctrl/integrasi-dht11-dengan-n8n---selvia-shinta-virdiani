#include <WiFi.h>
#include <PubSubClient.h>

#include <ArduinoJson.h>
#include "DHT.h"

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
unsigned long lastSend = 0;
unsigned long lastReconnectAttempt = 0;

// ================= WIFI =================
const char *ssid = "Oort";
const char *password = "hendryk1723";

// ================= MQTT =================
const char *mqtt_server = "broker.emqx.io";
const char *dht_topic = "selviashintavirdiani/dht-data";

WiFiClient espClient;
PubSubClient client(espClient);

// ================== CALLBACK FUNCTION ===================
void callback(char *topic, byte *payload, unsigned int length)
{
}

// ================= CONNECT WIFI =================
void setupWiFi()
{

  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ================= RECONNECT MQTT =================
void reconnect()
{

  if (millis() - lastReconnectAttempt > 2000)
  {

    lastReconnectAttempt = millis();

    Serial.print("Connecting MQTT...");

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

// ================= SETUP =================
void setup()
{

  Serial.begin(9600);

  setupWiFi();
  dht.begin();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.setBufferSize(512);
}

// ================= LOOP =================
void loop()
{

  // ===== WIFI =====
  if (WiFi.status() != WL_CONNECTED)
  {

    Serial.println("WiFi reconnect...");
    setupWiFi();
  }

  // ===== MQTT =====
  if (!client.connected())
  {
    reconnect();
  }

  client.loop();
  // ===== PUBLISH MQTT =====
  if (millis() - lastSend > 2000)
  {
    lastSend = millis();
    StaticJsonDocument<200> doc;
    float suhu = dht.readTemperature();
    float kelembapan = dht.readHumidity();
    Serial.print("Suhu: ");
    Serial.print(suhu);
    Serial.print("°C  Kelembapan: ");
    Serial.print(kelembapan);
    doc["suhu"] = suhu;
    doc["kelembapan"] = kelembapan;

    char buffer[256];

    serializeJson(doc, buffer);

    Serial.print("Publish: ");
    Serial.println(buffer);

    if (client.publish(dht_topic, buffer))
    {
      Serial.println("Publish sukses");
    }
    else
    {
      Serial.println("Publish gagal");
    }
  }
}