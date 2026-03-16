#include <Arduino.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ─── TTGO T-BEAM PIN MAP ─────────────────────────────────────────
#define LORA_SCK    5
#define LORA_MISO   19
#define LORA_MOSI   27
#define LORA_CS     18
#define LORA_RST    23
#define LORA_DIO0   26

// ─── LORA CONFIG — MUST MATCH SENDER ─────────────────────────────
#define LORA_FREQ       915E6
#define LORA_SF         9
#define LORA_BW         125E3
#define LORA_SYNC       0x12

// ─── RECEIVER HARDCODED COORDS ───────────────────────────────────
#define RECEIVER_LAT    -23.123456   // replace with your actual coords
#define RECEIVER_LON    -46.123456

// ─── WIFI + JSONBIN ───────────────────────────────────────────────
#define WIFI_SSID       "your_ssid"
#define WIFI_PASSWORD   "your_password"
#define JSONBIN_BIN_ID  "your_bin_id"
#define JSONBIN_API_KEY "your_api_key"
#define JSONBIN_URL     "https://api.jsonbin.io/v3/b/" JSONBIN_BIN_ID

// ─── SETUP ───────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // LoRa init
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("[LORA] Init failed");
    while (true);
  }

  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setSyncWord(LORA_SYNC);
  LoRa.receive(); // put radio in continuous RX mode

  Serial.println("[LORA] Listening...");

  // WiFi init
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WIFI] Failed — running LoRa-only");
  } else {
    Serial.printf("\n[WIFI] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
  }
}

// ─── POST TO JSONBIN ─────────────────────────────────────────────
void postToJsonBin(String payload) {
  bool wifiFailPrinted = false;

  // in postToJsonBin:
  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiFailPrinted) {
      Serial.println("[WIFI] Not connected, skipping POST");
      wifiFailPrinted = true;
    }
    return;
  }

  HTTPClient http;
  http.begin(JSONBIN_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Master-Key", JSONBIN_API_KEY);
  http.addHeader("X-Bin-Versioning", "false"); // overwrite, don't version

  int code = http.PUT(payload);
  Serial.printf("[HTTP] Response: %d\n", code);
  http.end();
}

// ─── LOOP ────────────────────────────────────────────────────────
void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return;

  // Read raw packet
  String received = "";
  while (LoRa.available()) {
    received += (char)LoRa.read();
  }

  // Signal metrics — read immediately after packet
  int rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();

  Serial.printf("[RX] %s | RSSI: %d | SNR: %.2f\n", received.c_str(), rssi, snr);

  // Build full payload for JSONBin
  char full[320];
  snprintf(full, sizeof(full),
    "{\"sender\":%s,\"receiver\":{\"lat\":%.6f,\"lon\":%.6f},\"rssi\":%d,\"snr\":%.2f}",
    received.c_str(),
    RECEIVER_LAT,
    RECEIVER_LON,
    rssi,
    snr
  );

  postToJsonBin(String(full));
}