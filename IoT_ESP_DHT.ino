#include <ESP8266WiFi.h>
#include <PubSubClient.h>             // AWS IoT MQTT
#include <WiFiClientSecureBearSSL.h>  // AWS IoT TLS (BearSSL)
#include <WiFiClientSecure.h>         // Telegram HTTPS (default WiFiClientSecure)
#include <ArduinoJson.h>
#include <time.h>  // NTP
#include <DHT.h>
#include <EEPROM.h>

// ───── Wi-Fi Credentials ─────
const char* ssid = "WIFI1";
const char* password = "dsa54321";
const unsigned long WIFI_RETRY_DELAY_MS = 5000;  // 5 seconds retry

// ───── AWS IoT Configuration ─────
const char* iotEndpoint = "a1iprve8c3g3fm-ats.iot.us-east-1.amazonaws.com";
const char* awsMqttTopic = "sensor/dht";

// ───── Telegram Configuration ─────
const char* telegramBotToken = "8039173850:AAG7fRy73-977Ll4ijIOVnL2Rov9gauSmJo";
const long telegramChatId = 251752990;
const char* telegramHost = "api.telegram.org";
WiFiClientSecure telegramClient;  // for Telegram HTTPS

// ───── Sensor & Pin ─────
#define DHTPIN D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ───── EEPROM Settings ─────
const int ADDR_READING_COUNT = 0;
unsigned long readingCount = 0;

// ───── TLS Certificates for AWS IoT (PROGMEM) ─────
static const char ROOT_CA_AWS[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

static const char DEVICE_CERT_AWS[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----
MIIDWTCCAkGgAwIBAgIUe/TcIv9Ah//MyVv8Lh+deHcz7RowDQYJKoZIhvcNAQEL
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI1MDYwNDA5MzQw
NloXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAM3zcaKw2mmjQryt0nW8
dE5C2pV5Yu4FlzazIUHmOHqmh55k6wdC7Pa9qOBO8N7z7vpBcb79wJLoMVq99jYR
Y5IZnFLbG8gU5Lm+Agkrnqtxgb5zDi7d0tmV3udTvtvMl4kBnpxIwaaaDVxhi2N2
O3d8ArHFptg4XNjfOMmd9rugpWz9uw/S15BnlOF6BUju9APhwU94aEjswsJ5Hmwk
K3b5p4ahAgrGpdOBWcMYPthoSLKF94qqsSWDdRmZpXDyC+JYD0C3ldy9G/0TP+iv
svNNInd/XBC/GXqXLHbUHzjiF8N/IoQtXA3dDVRaNV7mRB19eXlbJ/lqCcmRKvrt
Us8CAwEAAaNgMF4wHwYDVR0jBBgwFoAUKjyuCpvwuDIBlwsbz3jH/x47EdswHQYD
VR0OBBYEFCeeSn07X0WkRAs7rfNgIcMwvS2UMAwGA1UdEwEB/wQCMAAwDgYDVR0P
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQB9//nUXiLz7FEWgu+WmHrSIIi1
8r4GI12d5o3Z2nOy4k9aQK4A7xrFc/j67vf5NX/hOaiAMd7ykjhRm3vNOAtfhOzN
zBIWTXCqXZiB6ZhWBM6Eunheqtdx1UmRr4pDM4gVEW50iIs6c2bS39JJkY90A7g8
64hvsXBDstTfHPCl2KbeJ2ax0ZNxT6CEoGrVz5wWn8rz66bl7p3L49wHLlNaLcaN
iJAJBZyCZrHScPhVEUm5gWVEAYsYLnK3ZL7h3wmhDPS/zWgofvuydVh4itn9KCVp
EisIWMS0N4r68T6WSu5XgXtxmLaSEtpqbgM3Xu3xZpUN7/s642OBiezTKj3z
-----END CERTIFICATE-----
)KEY";

static const char DEVICE_KEY_AWS[] PROGMEM = R"KEY(
-----BEGIN RSA PRIVATE KEY-----
MIIEpQIBAAKCAQEAzfNxorDaaaNCvK3Sdbx0TkLalXli7gWXNrMhQeY4eqaHnmTr
B0Ls9r2o4E7w3vPu+kFxvv3AkugxWr32NhFjkhmcUtsbyBTkub4CCSueq3GBvnMO
Lt3S2ZXe51O+28yXiQGenEjBppoNXGGLY3Y7d3wCscWm2Dhc2N84yZ32u6ClbP27
D9LXkGeU4XoFSO70A+HBT3hoSOzCwnkebCQrdvmnhqECCsal04FZwxg+2GhIsoX3
iqqxJYN1GZmlcPIL4lgPQLeV3L0b/RM/6K+y800id39cEL8ZepcsdtQfOOIXw38i
hC1cDd0NVFo1XuZEHX15eVsn+WoJyZEq+u1SzwIDAQABAoIBAED4svmzN0QqOAyT
/Zgc+sgRuSl8oOQcKWcdPbmvalPvI0up5KdmyqDQlm0lGkILzHFwofSx0sXFsIRC
92B5PeNCZxmQfAQz6zBZrAS+mRDW/ypY27mXS0RPToHF3qkysv+K3kHJhiYL5Xkm
YLXg/8OjAKqagxclOM1GJoEcb1EjPNgKNRXPit7mGRnCyYDXJ4lM8fFmSzdG1fdn
pEyTc3T1czTysSpIax0VZjZ12d7fcAXIL54Q5dvpLSnqMCJIcDvTrsXBbpvEM3R2
z+MoOvwRUsfJhGJ4N3qUtCjTfYAVRtbdEk/2uFEAbMyRP0sfnbMmfu5/wNmv8nhk
uZj9tBECgYEA8ifznPbzNV85x9Ny1rH//gNjQAxPEdBbNk0iKLDz4HIv6jgBSEPy
m/05j21xV3NwPe6k4k7pP5QPsd/t9gmBn63zoXK4gC16dBFNLFj1yrzpeNujZWd9
l+LukwOtaCXGzLrnPX6gPwGijMIlZ4Li4UmW6MtyA5ypwmausgiX/RcCgYEA2bmd
zCCd81BuhbLPe+h2IbeucgcxJ9hknGp5GdwHca7VJ1rb07etgBrL8N6SjlVWwQoV
Sxeq9WpJg5FYDktvpgnGLZ9v6xZ0hz+vF0OZfS2IRBL/A99+k00kDV1QQ/N8SHf/
CQ2ckBflSoLNZyUxTRNFE6UZOtSTQW0fSx+8GwkCgYEAinyerd3tKVDUUptyyaXy
qOp3EGH5tk5aW6uxJWRNlMa48FInKZTyYpNnH8ePUlwKjOC2G1bVvi6G60sNY+/7
2b453tMlAOkBZu+eGwalStTPEPdLcurEwOBfYGRx/2XbU6pwJJMOQfpFZAqEKbaI
2h6j127CPZ6S10KyFc8kXPECgYEAr2dohxEn7uO3hqK9oTdwJE3UjizZHx6oP5NP
qNOoc5/EPYZnXzO05WWxM4Y8T8rUr4QuD2cr5bcRLpujczC26+8n541xHtXiXyuh
JX7iYwSRqTYcmMQvNwCIsPOiPHwmfkOeBW8f2L5HjTW/wP8nrs59cgwqPUkQsT72
XRFd+/ECgYEAt/+2CA4IzSckq0NHGzauOiwRXwl0wA3Em2KR0uI3QMJUQL8YRIWc
VTteclSFWst1ISwz9IeFbqEc/FEw9jxOTtT7ec+w3q89UQ0xvZxhUohrPQocckjX
Jk4TgXu5Ia2FTy5bvIlbMVI8+CSXJJPrBTsTy1RGbibabHm6k9xDq6A=
-----END RSA PRIVATE KEY-----
)KEY";

// ───── BearSSL objects for AWS IoT ─────
BearSSL::X509List awsRootCertList;
BearSSL::X509List awsDeviceCertList;
BearSSL::PrivateKey awsDevicePrivKey;
BearSSL::WiFiClientSecure awsIoTConnectionClient;
PubSubClient mqtt(awsIoTConnectionClient);

long lastAWSRecAttempt = 0;

// ───** Helper: URL-encode for Telegram **─
String urlencode(const String& s) {
  String enc;
  for (auto c : s) {
    if (c == ' ') enc += "%20";
    else if (c == '\n') enc += "%0A";
    else enc += c;
  }
  return enc;
}

// ───** Send Telegram Message **─
void sendTelegramMessage(const String& message) {
  // If Wi-Fi is down, skip
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Telegram: Wi-Fi not connected. Skipping message."));
    return;
  }

  // 1) Disable certificate checks for Telegram
  telegramClient.setInsecure();

  // 2) Check DNS
  IPAddress telegramIP;
  if (!WiFi.hostByName(telegramHost, telegramIP)) {
    Serial.println(F("Telegram: DNS lookup failed for api.telegram.org"));
    return;
  }
  Serial.print(F("Telegram: api.telegram.org → "));
  Serial.println(telegramIP);

  // 3) Connect to Telegram (TLS) on port 443
  Serial.println(F("→ Connecting to Telegram…"));
  if (!telegramClient.connect(telegramHost, 443)) {
    Serial.println(F("Telegram: Connection failed!"));
    return;
  }

  // 4) Build GET request
  String url = "/bot" + String(telegramBotToken)
               + "/sendMessage?chat_id=" + String(telegramChatId)
               + "&text=" + urlencode(message);

  Serial.print(F("Telegram: GET "));
  Serial.println(url);

  telegramClient.print(String("GET ") + url + " HTTP/1.1\r\n"
                       + "Host: " + telegramHost + "\r\n"
                       + "User-Agent: ESP8266\r\n"
                       + "Connection: close\r\n\r\n");

  // 5) Wait up to 5 seconds for response
  unsigned long timeout = millis();
  while (telegramClient.connected() && !telegramClient.available()) {
    if (millis() - timeout > 5000) {
      Serial.println(F("Telegram: Timeout waiting for response"));
      telegramClient.stop();
      return;
    }
    delay(50);
  }

  // 6) Skip HTTP headers
  while (telegramClient.connected()) {
    String line = telegramClient.readStringUntil('\n');
    if (line == "\r") break;
  }

  // 7) Print body
  while (telegramClient.available()) {
    String line = telegramClient.readStringUntil('\n');
    Serial.println(line);
  }

  telegramClient.stop();
  Serial.println(F("✓ Telegram message sent\n"));
}

// ─────────** Connect to Wi-Fi (with retries) **─────────
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("\nAttempting to connect to Wi-Fi “"));
    Serial.print(ssid);
    Serial.print(F("”…"));
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
      Serial.print('.');
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(F("\n❌ Failed to join Wi-Fi on this attempt."));
      Serial.print(F("Retrying in "));
      Serial.print(WIFI_RETRY_DELAY_MS / 1000);
      Serial.println(F(" seconds…"));
      WiFi.disconnect(true);
      delay(WIFI_RETRY_DELAY_MS);
    }
  }
  Serial.println(F("\n✅ Wi-Fi connected"));
  Serial.print(F("   IP Address: "));
  Serial.println(WiFi.localIP());
}

// ─────────** NTP Sync (UTC) **─────────
void syncNTPTime() {
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");
  Serial.print(F("Waiting for NTP sync…"));
  time_t now = time(nullptr);
  unsigned long start = millis();
  while (now < 1704067200UL && millis() - start < 30000) {  // 1704067200 ~ 2024-01-01
    Serial.print('.');
    delay(500);
    now = time(nullptr);
  }
  Serial.println();
  if (now < 1704067200UL) {
    Serial.println(F("❌ NTP sync FAILED or time still in the past. TLS may fail!"));
  } else {
    Serial.println(F("✅ NTP sync OK."));
  }
  struct tm ti;
  gmtime_r(&now, &ti);
  Serial.printf("   UTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                ti.tm_hour, ti.tm_min, ti.tm_sec);
}

// ─────────** Connect to AWS IoT MQTT over TLS (BearSSL) **─────────
bool connectAWSIoTMQTT() {
  Serial.print(F("Attempting AWS IoT MQTT connection… "));
  String rawMac = WiFi.macAddress();
  rawMac.replace(":", "");
  String clientId = "esp8266-" + rawMac;
  Serial.print(F("Client ID="));
  Serial.println(clientId);

  if (mqtt.connect(clientId.c_str())) {
    Serial.println(F("✅ AWS IoT MQTT connected!"));
    return true;
  }
  Serial.print(F("❌ AWS IoT MQTT connect FAILED, rc="));
  Serial.println(mqtt.state());
  Serial.println(F("   (Check AWS certs, policy, NTP time, endpoint)"));

  if (!awsIoTConnectionClient.connected() && WiFi.status() == WL_CONNECTED) {
    // Print BearSSL error if available
    char errBuf[200];
    memset(errBuf, 0, sizeof(errBuf));
    int brsslErr = awsIoTConnectionClient.getLastSSLError(errBuf, sizeof(errBuf) - 1);
    if (brsslErr != 0) {
      Serial.print(F("   BearSSL Err 0x"));
      if (brsslErr < 0x10) Serial.print('0');
      Serial.println(brsslErr, HEX);
      Serial.print(F("   BearSSL Msg: "));
      Serial.println(errBuf);
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(10);
  randomSeed(micros());

  Serial.println(F("\n\n=== ESP8266 AWS IoT & Telegram DHT Sensor Demo ==="));

  // ── 1) EEPROM: load last reading count ──
  EEPROM.begin(512);
  EEPROM.get(ADDR_READING_COUNT, readingCount);
  Serial.print(F("Last reading count from EEPROM: #"));
  Serial.println(readingCount);

  // ── 2) DHT init ──
  dht.begin();
  Serial.println(F("DHT sensor initialized."));

  // ── 3) Wi-Fi → NTP → TLS setup ──
  connectWiFi();
  syncNTPTime();

  // ── 4) Load AWS IoT certificates into BearSSL lists ──
  awsRootCertList.append(ROOT_CA_AWS);
  awsDeviceCertList.append(DEVICE_CERT_AWS);
  awsDevicePrivKey.parse(DEVICE_KEY_AWS);

  // ── 5) Configure BearSSL client for AWS IoT ──
  awsIoTConnectionClient.setTrustAnchors(&awsRootCertList);
  awsIoTConnectionClient.setClientRSACert(&awsDeviceCertList, &awsDevicePrivKey);

  // ── 6) Configure PubSubClient (MQTT) to use BearSSL → connect ──
  mqtt.setServer(iotEndpoint, 8883);
  if (connectAWSIoTMQTT()) {
    StaticJsonDocument<192> doc;
    doc["message"] = "ESP8266 connected to AWS IoT!";
    doc["readingCount"] = readingCount;
    doc["timestamp_utc"] = (unsigned long)time(nullptr);
    char buf[192];
    serializeJson(doc, buf);
    if (mqtt.publish(awsMqttTopic, buf)) {
      Serial.print(F("Published initial AWS message to “"));
      Serial.print(awsMqttTopic);
      Serial.println(F("” ✅"));
    } else {
      Serial.println(F("❌ Initial AWS publish FAILED"));
    }
  }
  lastAWSRecAttempt = millis();
}

void loop() {
  // ── 1) Maintain AWS MQTT connection (re‐connect every 5 seconds if dropped) ──
  if (!mqtt.connected()) {
    unsigned long now_ms = millis();
    if (now_ms - lastAWSRecAttempt > 5000) {
      lastAWSRecAttempt = now_ms;
      Serial.println(F("AWS MQTT connection lost. Attempting reconnect…"));
      if (WiFi.status() != WL_CONNECTED) connectWiFi();
      connectAWSIoTMQTT();
    }
  } else {
    mqtt.loop();
  }

  // ── 2) Every 30 seconds: read DHT → publish to AWS → send Telegram ──
  static unsigned long lastSensorPublish = 0;
  if (millis() - lastSensorPublish > 30000UL) {
    lastSensorPublish = millis();

    // a) Increment & persist readingCount
    readingCount++;
    EEPROM.put(ADDR_READING_COUNT, readingCount);
    if (!EEPROM.commit()) {
      Serial.println(F("EEPROM commit failed!"));
    }

    // b) Read DHT
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    time_t ts = time(nullptr);

    // c) Format timestamp
    struct tm ti;
    gmtime_r(&ts, &ti);
    char tsBuf[25];
    strftime(tsBuf, sizeof(tsBuf), "%Y-%m-%d %H:%M:%S", &ti);

    // e) Build AWS JSON payload
    String dhtStatus = "OK";
    if (isnan(humidity) || isnan(temperature)) {
      dhtStatus = "DHT Read Failed";
    }
    StaticJsonDocument<256> awsDoc;
    awsDoc["message"] = "Sensor Reading";
    if (!isnan(temperature)) awsDoc["temp"] = temperature;
    if (!isnan(humidity)) awsDoc["hum"] = humidity;
    awsDoc["dht_status"] = dhtStatus;
    awsDoc["readingCount"] = readingCount;
    awsDoc["timestamp"] = ts;
    char awsPayload[256];
    serializeJson(awsDoc, awsPayload);

    // f) Publish to AWS
    Serial.print(F("AWS Publish: "));
    Serial.println(awsPayload);
    if (mqtt.publish(awsMqttTopic, awsPayload)) {
      Serial.println(F("✅ AWS publish OK"));
    } else {
      Serial.println(F("❌ AWS publish FAILED"));
    }

    // g) Prepare Telegram text
    String telMsg = "Reading #" + String(readingCount) + "\n";
    telMsg += "Date: " + String(tsBuf) + "\n";
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println(F("⚠️  DHT read failed"));
      telMsg += "⚠️  DHT read failed";
    } else {
      Serial.printf(" → DHT: %.1f°C, %.1f%%\n", temperature, humidity);
      telMsg += "🌡️ Temp: " + String(temperature, 1) + "°C\n";
      telMsg += "💧 Hum: " + String(humidity, 1) + "%";
    }

    // h) **Before sending Telegram**, free up BearSSL heap by stopping AWS TLS**:
    if (awsIoTConnectionClient.connected()) {
      awsIoTConnectionClient.stop();
      mqtt.disconnect();
      Serial.println(F("→ Freed AWS TLS session to open Telegram TLS (frees ~32 KB heap)"));
    }

    // i) Send Telegram message
    sendTelegramMessage(telMsg);

    // j) After telegram is done, immediately reopen AWS MQTT:
    if (WiFi.status() == WL_CONNECTED) {
      // Give BearSSL a moment to reclaim heap
      delay(100);
      connectAWSIoTMQTT();
    }
  }

  delay(10);
}
