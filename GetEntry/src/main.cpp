#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClient.h>

constexpr int irReceiver = 4;

// SD (VSPI)
static constexpr int SD_CS   = 27;
static constexpr int SD_SCK  = 13;
static constexpr int SD_MISO = 32;
static constexpr int SD_MOSI = 14;

// Если SD начнёт глючить — поставь 4000000 или 1000000
static constexpr uint32_t SD_SPI_HZ = 8000000;
SPIClass spi(VSPI);

// WiFi
static constexpr char ssid[] = "Habitacion 152";
static constexpr char pass[] = "WhSVXCn4";

// Server (ноут) — ВАЖНО: IP ноута в той же подсети что ESP
static const char* SERVER_HOST = "192.168.0.102";
static constexpr uint16_t SERVER_PORT = 5000;

// Таймауты
static constexpr uint32_t WIFI_TIMEOUT_MS = 15000;
static constexpr uint32_t HTTP_TIMEOUT_MS = 25000;

bool initSD() {
  spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spi, SD_SPI_HZ)) {
    Serial.println("SD init FAIL");
    return false;
  }
  Serial.println("SD init OK");
  return true;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting to WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, ESP IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("WiFi FAIL (timeout)");
  }
}

// RAW POST: /upload/<filename> , body = bytes
bool uploadFileRaw(const char* filePathOnSd) {
  File f = SD.open(filePathOnSd, FILE_READ);
  if (!f) {
    Serial.printf("Open FAIL: %s\n", filePathOnSd);
    return false;
  }

  String filename = String(filePathOnSd);
  int slash = filename.lastIndexOf('/');
  if (slash >= 0) filename = filename.substring(slash + 1);

  uint32_t size = (uint32_t)f.size();
  if (size == 0) {
    Serial.printf("Empty file: %s\n", filePathOnSd);
    f.close();
    return false;
  }

  WiFiClient client;
  client.setTimeout(HTTP_TIMEOUT_MS / 1000);

  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println("Connect server FAIL");
    f.close();
    return false;
  }

  // noDelay AFTER connect (иначе были ошибки setSocketOption)
  client.setNoDelay(true);

  client.print("POST /upload/");
  client.print(filename);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.print(SERVER_HOST);
  client.print(":");
  client.println(SERVER_PORT);
  client.println("Content-Type: application/octet-stream");
  client.print("Content-Length: ");
  client.println(size);
  client.println("Connection: close");
  client.println();

  static uint8_t buf[16384]; // 16KB
  uint32_t sent = 0;

  while (true) {
    int n = f.read(buf, sizeof(buf));
    if (n <= 0) break;

    size_t w = client.write(buf, n);
    if (w != (size_t)n) {
      Serial.printf("Socket write short (%u/%u) -> FAIL\n", (unsigned)w, (unsigned)n);
      f.close();
      client.stop();
      return false;
    }
    sent += (uint32_t)w;
    delay(0);
  }

  f.close();

  // Читаем статусную строку
  String statusLine;
  unsigned long t0 = millis();
  while (client.connected() && millis() - t0 < HTTP_TIMEOUT_MS) {
    if (client.available()) {
      statusLine = client.readStringUntil('\n');
      break;
    }
    delay(5);
  }
  client.stop();

  bool ok = statusLine.indexOf("200") >= 0;
  if (!ok) {
    Serial.print("Upload bad status: ");
    Serial.println(statusLine);
    Serial.printf("Sent %u bytes (expected %u)\n", (unsigned)sent, (unsigned)size);
  }

  return ok;
}

// 000001.jpg, 000002.jpg ... пока не найдём следующий
bool uploadAllPhotosSequential() {
  int sent = 0;

  for (int i = 1; ; i++) {
    char path[32];
    snprintf(path, sizeof(path), "/%06d.jpg", i);

    if (!SD.exists(path)) {
      Serial.printf("Stop: %s not found\n", path);
      break;
    }

    Serial.printf("Uploading %s\n", path);

    bool ok = false;
    for (int attempt = 1; attempt <= 4; attempt++) {
      if (uploadFileRaw(path)) {
        ok = true;
        break;
      }
      Serial.printf("Retry %d for %s\n", attempt, path);
      delay(300);
    }

    if (!ok) {
      Serial.printf("GIVE UP on %s\n", path);
      return false;
    }

    sent++;
    delay(0);
  }

  Serial.printf("Upload done. sent=%d\n", sent);
  return sent > 0;
}

bool finalizeOnServer() {
  WiFiClient client;
  client.setTimeout(HTTP_TIMEOUT_MS / 1000);

  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    Serial.println("Finalize connect FAIL");
    return false;
  }

  client.setNoDelay(true);

  client.println("POST /finalize HTTP/1.1");
  client.print("Host: ");
  client.print(SERVER_HOST);
  client.print(":");
  client.println(SERVER_PORT);
  client.println("Content-Length: 0");
  client.println("Connection: close");
  client.println();

  String statusLine;
  unsigned long t0 = millis();
  while (client.connected() && millis() - t0 < HTTP_TIMEOUT_MS) {
    if (client.available()) {
      statusLine = client.readStringUntil('\n');
      break;
    }
    delay(5);
  }
  client.stop();

  Serial.print("Finalize status: ");
  Serial.println(statusLine);

  return statusLine.indexOf("200") >= 0;
}

void logic() {
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) return;

  if (!initSD()) return;

  bool ok = uploadAllPhotosSequential();
  if (ok) finalizeOnServer();
}

void setup() {
  Serial.begin(115200);
  delay(50);

  auto cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("IR received signal. Starting transfer...");
    delay(200);
    logic();
  } else {
    Serial.println("Power on / reset");
  }

  pinMode(irReceiver, INPUT);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)irReceiver, 0);

  Serial.println("Sleeping...");
  esp_deep_sleep_disable_rom_logging();
  esp_deep_sleep_start();
}

void loop() {}