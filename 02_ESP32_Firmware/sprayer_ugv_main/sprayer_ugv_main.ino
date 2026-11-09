#include "secrets.h"
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <DFRobot_BMM150.h>
#include <Wire.h>
#include <TinyGPS++.h>

// =====================================================
// GPS
// =====================================================
TinyGPSPlus gps;
HardwareSerial GPS(2);

// =====================================================
// KOMPAS BMM150
// =====================================================
DFRobot_BMM150_I2C compass(&Wire, 0x13);

float heading = 0.0;
float smoothHeading = 0.0;
bool compassReady = false;

// =====================================================
// WIFI
// =====================================================


// =====================================================
// WEBSOCKET
// =====================================================
WebSocketsClient webSocket;

const char* server = "192.168.1.11";
const int port = 8080;

bool websocketConnected = false;

// =====================================================
// JOYSTICK
// x = motor kiri
// y = motor kanan
// Rentang -255 sampai +255
// =====================================================
int motorKiri = 0;
int motorKanan = 0;

unsigned long lastJoystick = 0;
const unsigned long joystickTimeout = 500;

bool motorDiizinkan = false;

// =====================================================
// IBT-2 KANAN
// =====================================================
#define RPWM_KANAN 19
#define LPWM_KANAN 18
#define R_EN_KANAN 27
#define L_EN_KANAN 14

// =====================================================
// IBT-2 KIRI
// =====================================================
#define RPWM_KIRI 25
#define LPWM_KIRI 26
#define R_EN_KIRI 32
#define L_EN_KIRI 33

// =====================================================
// PROTOTIPE FUNGSI
// =====================================================
void setupMotor();
void matikanDriver();
void aktifkanDriver();
void kontrolMotorKanan(int kecepatan);
void kontrolMotorKiri(int kecepatan);

void setupGPS();
void bacaGPS();

void setupCompass();
float bacaCompass();

void setupWiFi();
void setupWebsockets();

void webSocketEvent(
  WStype_t type,
  uint8_t* payload,
  size_t length
);

void kirimData();

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  // Utamakan keselamatan motor.
  setupMotor();

  Serial.println();
  Serial.println("ESP32 mulai");
  Serial.println("Motor dalam kondisi OFF");

  setupGPS();
  setupCompass();
  setupWiFi();
  setupWebsockets();
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  webSocket.loop();

  // Hentikan motor jika perintah kedaluwarsa.
  if (lastJoystick == 0 ||
      millis() - lastJoystick > joystickTimeout ||
      !websocketConnected) {

    motorKiri = 0;
    motorKanan = 0;
    matikanDriver();
  }

  // Perbarui kontrol motor.
  kontrolMotorKanan(motorKanan);
  kontrolMotorKiri(motorKiri);

  // Sensor.
  bacaGPS();

  if (compassReady) {
    heading = bacaCompass();
  }

  // Kirim data GPS dan kompas.
  kirimData();

  delay(100);
}

// =====================================================
// SETUP MOTOR
// =====================================================
void setupMotor() {
  // Tetapkan latch LOW sebelum OUTPUT.
  digitalWrite(R_EN_KANAN, LOW);
  digitalWrite(L_EN_KANAN, LOW);
  digitalWrite(R_EN_KIRI, LOW);
  digitalWrite(L_EN_KIRI, LOW);

  digitalWrite(RPWM_KANAN, LOW);
  digitalWrite(LPWM_KANAN, LOW);
  digitalWrite(RPWM_KIRI, LOW);
  digitalWrite(LPWM_KIRI, LOW);

  pinMode(R_EN_KANAN, OUTPUT);
  pinMode(L_EN_KANAN, OUTPUT);
  pinMode(R_EN_KIRI, OUTPUT);
  pinMode(L_EN_KIRI, OUTPUT);

  pinMode(RPWM_KANAN, OUTPUT);
  pinMode(LPWM_KANAN, OUTPUT);
  pinMode(RPWM_KIRI, OUTPUT);
  pinMode(LPWM_KIRI, OUTPUT);

  analogWrite(RPWM_KANAN, 0);
  analogWrite(LPWM_KANAN, 0);
  analogWrite(RPWM_KIRI, 0);
  analogWrite(LPWM_KIRI, 0);

  motorKiri = 0;
  motorKanan = 0;
  motorDiizinkan = false;

  matikanDriver();
}

// =====================================================
// MATIKAN DRIVER
// =====================================================
void matikanDriver() {
  // Nolkan PWM terlebih dahulu.
  analogWrite(RPWM_KANAN, 0);
  analogWrite(LPWM_KANAN, 0);
  analogWrite(RPWM_KIRI, 0);
  analogWrite(LPWM_KIRI, 0);

  // Nonaktifkan kedua driver.
  digitalWrite(R_EN_KANAN, LOW);
  digitalWrite(L_EN_KANAN, LOW);
  digitalWrite(R_EN_KIRI, LOW);
  digitalWrite(L_EN_KIRI, LOW);

  motorDiizinkan = false;
}

// =====================================================
// AKTIFKAN DRIVER
// =====================================================
void aktifkanDriver() {
  digitalWrite(R_EN_KANAN, HIGH);
  digitalWrite(L_EN_KANAN, HIGH);
  digitalWrite(R_EN_KIRI, HIGH);
  digitalWrite(L_EN_KIRI, HIGH);

  motorDiizinkan = true;
}

// =====================================================
// KONTROL MOTOR KANAN
// =====================================================
void kontrolMotorKanan(int kecepatan) {
  kecepatan = constrain(kecepatan, -255, 255);

  if (!motorDiizinkan) {
    analogWrite(RPWM_KANAN, 0);
    analogWrite(LPWM_KANAN, 0);
    return;
  }

  if (kecepatan > 0) {
    analogWrite(LPWM_KANAN, 0);
    analogWrite(RPWM_KANAN, kecepatan);
  }
  else if (kecepatan < 0) {
    analogWrite(RPWM_KANAN, 0);
    analogWrite(LPWM_KANAN, -kecepatan);
  }
  else {
    analogWrite(RPWM_KANAN, 0);
    analogWrite(LPWM_KANAN, 0);
  }
}

// =====================================================
// KONTROL MOTOR KIRI
// =====================================================
void kontrolMotorKiri(int kecepatan) {
  kecepatan = constrain(kecepatan, -255, 255);

  if (!motorDiizinkan) {
    analogWrite(RPWM_KIRI, 0);
    analogWrite(LPWM_KIRI, 0);
    return;
  }

  if (kecepatan > 0) {
    analogWrite(LPWM_KIRI, 0);
    analogWrite(RPWM_KIRI, kecepatan);
  }
  else if (kecepatan < 0) {
    analogWrite(RPWM_KIRI, 0);
    analogWrite(LPWM_KIRI, -kecepatan);
  }
  else {
    analogWrite(RPWM_KIRI, 0);
    analogWrite(LPWM_KIRI, 0);
  }
}

// =====================================================
// GPS
// =====================================================
void setupGPS() {
  GPS.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("GPS siap");
}

void bacaGPS() {
  while (GPS.available()) {
    gps.encode(GPS.read());
  }

  if (gps.location.isUpdated()) {
    Serial.print("Lat: ");
    Serial.print(gps.location.lat(), 6);

    Serial.print(" | Lng: ");
    Serial.print(gps.location.lng(), 6);

    Serial.print(" | Speed: ");
    Serial.print(gps.speed.kmph());

    Serial.print(" km/jam | Satelit: ");
    Serial.println(gps.satellites.value());
  }
}

// =====================================================
// KOMPAS BMM150
// =====================================================
void setupCompass() {
  Wire.begin(21, 22);

  // Jangan menunggu tanpa batas jika kompas tidak terdeteksi.
  if (compass.begin()) {
    Serial.println("Kompas gagal terdeteksi");
    compassReady = false;
    return;
  }

  compass.setOperationMode(BMM150_POWERMODE_NORMAL);

  compassReady = true;
  Serial.println("Kompas siap");
}

float bacaCompass() {
  sBmm150MagData_t magData =
      compass.getGeomagneticData();

  float newHeading =
      atan2(magData.x, magData.y) * 180.0 / PI;

  if (newHeading < 0) {
    newHeading += 360.0;
  }

  float diff = newHeading - smoothHeading;

  if (diff > 180.0) {
    diff -= 360.0;
  }

  if (diff < -180.0) {
    diff += 360.0;
  }

  const float alpha = 0.2;

  smoothHeading += alpha * diff;

  if (smoothHeading < 0) {
    smoothHeading += 360.0;
  }

  if (smoothHeading >= 360.0) {
    smoothHeading -= 360.0;
  }

  return smoothHeading;
}

// =====================================================
// WIFI
// =====================================================
void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Menghubungkan WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi terhubung");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// SETUP WEBSOCKET
// =====================================================
void setupWebsockets() {
  webSocket.begin(server, port, "/ws");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  Serial.println("WebSocket mulai");
}

// =====================================================
// EVENT WEBSOCKET
// =====================================================
void webSocketEvent(
  WStype_t type,
  uint8_t* payload,
  size_t length
) {
  switch (type) {
    case WStype_CONNECTED:
      websocketConnected = true;

      // Jangan otomatis menjalankan motor saat tersambung.
      motorKiri = 0;
      motorKanan = 0;
      lastJoystick = 0;
      matikanDriver();

      Serial.println("WebSocket connected");
      break;

    case WStype_DISCONNECTED:
      websocketConnected = false;

      motorKiri = 0;
      motorKanan = 0;
      lastJoystick = 0;
      matikanDriver();

      Serial.println("WebSocket disconnected");
      break;

    case WStype_TEXT: {
      StaticJsonDocument<200> doc;

      DeserializationError error =
          deserializeJson(doc, payload, length);

      if (error) {
        Serial.println("JSON error");
        break;
      }

      // Abaikan pesan selain joystick.
      if (doc["type"] != "joystick") {
        break;
      }

      // Periksa bahwa kedua nilai berupa integer.
      if (!doc["x"].is<int>() ||
          !doc["y"].is<int>()) {

        motorKiri = 0;
        motorKanan = 0;
        lastJoystick = 0;
        matikanDriver();
        break;
      }

      motorKiri =
          constrain(doc["x"].as<int>(), -255, 255);

      motorKanan =
          constrain(doc["y"].as<int>(), -255, 255);

      lastJoystick = millis();

      if (motorKiri == 0 && motorKanan == 0) {
        matikanDriver();
      }
      else {
        // Aktifkan driver hanya untuk perintah nonnol.
        aktifkanDriver();
      }

      Serial.printf(
        "Joystick | Kiri: %d | Kanan: %d\n",
        motorKiri, motorKanan
      );

      break;
    }

    default:
      break;
  }
}

// =====================================================
// KIRIM GPS DAN KOMPAS KE BROWSER
// =====================================================
void kirimData() {
  if (!websocketConnected || !webSocket.isConnected()) {
    return;
  }

  StaticJsonDocument<200> doc;

  if (gps.location.isValid()) {
    doc["lat"] = gps.location.lat();
    doc["lng"] = gps.location.lng();
  } else {
    doc["lat"] = nullptr;
    doc["lng"] = nullptr;
  }

  doc["speed"] = gps.speed.kmph();

  if (compassReady) {
    doc["heading"] = heading;
  } else {
    doc["heading"] = nullptr;
  }

  String data;
  serializeJson(doc, data);

  webSocket.sendTXT(data);
}
