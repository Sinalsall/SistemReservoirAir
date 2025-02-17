#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <NewPing.h>
#include <WiFi.h>

// === [Tambahkan Library GravityTDS] ===
#include <GravityTDS.h>
#include <esp_adc_cal.h>    // Agar ADC ESP32 bekerja akurat

// 1) Definisi Pin dan Parameter
#define TRIGGER_PIN     15
#define ECHO_PIN        4
#define TRIGGER_PIN2    14
#define ECHO_PIN2       2
#define TANK_HEIGHT     18   // Tinggi tangki reservoir (cm)
#define TANK_HEIGHT2    6

#define PUMP1_PIN       25
#define PUMP2_PIN       27
#define PUMP3_PIN       26
#define TDS_SENSOR_PIN  35   // TDS sensor (analog)

// Batas Histeresis Reservoir
#define WATER_LEVEL_ON_THRESHOLD   2   // Pompa 1 ON jika < 2 cm
#define WATER_LEVEL_OFF_THRESHOLD  4   // Pompa 1 OFF jika >= 6 cm
#define WATER_LEVEL3_ON_THRESHOLD  2   // Pompa 1 On jika < 1 cm
#define WATER_LEVEL3_OFF_THRESHOLD 4  // Pompa 2 OFF JIka >= 4 cm


// 2) Variabel Global dan Objek
// Pastikan variabel-variabel berikut tidak dideklarasikan dengan 'static'
// sehingga dapat diakses oleh WebServerHandler (melalui deklarasi 'extern')
GravityTDS gravityTds;

volatile float g_reservoirLevel1 = 0;   // Ketinggian air reservoir (cm)
volatile float g_reservoirLevel2 = 0;     // Ketinggian air sumur (cm)
volatile bool  g_sumurPenuh     = false;
volatile float g_tdsValue       = 0;      // Nilai TDS (ppm)

bool pump1State = false;  // OFF = false, ON = true
bool pump2State = false;
bool pump3State = false;

TickType_t lastChangePump1 = 0;
TickType_t lastChangePump2 = 0;
//TickType_t lastChangePump3 = 0;

static const TickType_t MIN_CHANGE_INTERVAL = 2000 / portTICK_PERIOD_MS; // 2 detik

// 3) Deklarasi Task
void TaskUltrasonic(void *pvParameters);
void TaskUltrasonic2(void *pvParameters);
void TaskTDS(void *pvParameters);
void TaskControl(void *pvParameters);

// Sertakan header WebServerHandler dan buat instance
#include "WebServerHandler.h"
WebServerHandler webHandler;

// 4) Setup
// Ganti dengan kredensial WiFi Anda
const char* ssid = "Samsung Galaxy";
const char* password = "12345678";

void setup() {
  Serial.begin(115200);

  // Inisialisasi pin relay (pompa)
  pinMode(PUMP1_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);
  pinMode(PUMP3_PIN, OUTPUT);
  digitalWrite(PUMP1_PIN, HIGH); 
  digitalWrite(PUMP2_PIN, HIGH);
  digitalWrite(PUMP3_PIN, HIGH);

  // Inisialisasi pin sensor ultrasonik
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIGGER_PIN2, OUTPUT);
  pinMode(ECHO_PIN2, INPUT);

  // Konfigurasi GravityTDS
  gravityTds.setPin(TDS_SENSOR_PIN);
  gravityTds.setAdcRange(4096);
  gravityTds.setAref(3.3);
  gravityTds.begin();

  // Inisialisasi WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Tersambung. IP Address: ");
  Serial.println(WiFi.localIP());

  // Mulai Web Server
  webHandler.begin();

  // Membuat task RTOS
  xTaskCreate(TaskUltrasonic, "Ultrasonic", configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);
  xTaskCreate(TaskUltrasonic2, "Ultrasonic2", configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);
  xTaskCreate(TaskTDS, "TDS", configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);  // Uncomment jika sensor TDS digunakan
  xTaskCreate(TaskControl, "Control", configMINIMAL_STACK_SIZE * 4, NULL, 2, NULL);
}

void loop() {
  // Web server diproses secara berkala pada loop utama
  webHandler.handleClient();
  delay(10);
}

// 5) TaskUltrasonic: Membaca ketinggian air reservoir
void TaskUltrasonic(void *pvParameters) {
  (void) pvParameters;
  const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
  
  while (1) {
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH);
    int distanceCm = duration * 0.034 / 2;
    int distancefinal1 = TANK_HEIGHT - distanceCm;
    Serial.print("[Ultrasonic] Tinggi air reservoir: ");
    Serial.print(distancefinal1);
    Serial.println(" cm");
    
    g_reservoirLevel1 = distancefinal1;
    vTaskDelay(xDelay);
  }
}

// 6) TaskUltrasonic2: Membaca ketinggian air sumur
void TaskUltrasonic2(void *pvParameters) {
  (void) pvParameters;
  const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
  
  while (1) {
    digitalWrite(TRIGGER_PIN2, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN2, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN2, LOW);
    
    long duration2 = pulseIn(ECHO_PIN2, HIGH);
    int distanceCm2 = duration2 * 0.034 / 2;
    int distancefinal2 = TANK_HEIGHT2 - distanceCm2;
    
    Serial.print("[Ultrasonic2] Tinggi air sumur: ");
    Serial.print(distancefinal2);
    Serial.println(" cm");
    
    g_reservoirLevel2 = distancefinal2;
    vTaskDelay(xDelay);
  }
}

// 7) TaskTDS: Membaca nilai TDS (menggunakan library GravityTDS)
void TaskTDS(void *pvParameters) {
  (void) pvParameters;
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS;
  
  while (1) {
    gravityTds.update();
    g_tdsValue = gravityTds.getTdsValue();
    Serial.print("[TDS] Nilai TDS: ");
    Serial.print(g_tdsValue, 2);
    Serial.println(" ppm");
    
    vTaskDelay(xDelay);
  }
}

// 8) TaskControl: Mengendalikan pompa berdasarkan logika sensor
void TaskControl(void *pvParameters) {
  (void) pvParameters;
  const TickType_t xDelay = 200 / portTICK_PERIOD_MS;

  digitalWrite(PUMP3_PIN, LOW);
  pump3State = true;
  Serial.println("[Control] Pompa 3: ON (permanen)");
  
  while (1) {
    TickType_t now = xTaskGetTickCount();
    
    // Logika Pompa 1 (Air Masuk Reservoir)
    bool shouldPump1On = pump1State;
    if (g_reservoirLevel1 < WATER_LEVEL_ON_THRESHOLD) {
      shouldPump1On = true;
    } else if (g_reservoirLevel1 >= WATER_LEVEL_OFF_THRESHOLD) {
      shouldPump1On = false;
    }
    if (shouldPump1On != pump1State) {
      if ((now - lastChangePump1) > MIN_CHANGE_INTERVAL) {
        pump1State = shouldPump1On;
        lastChangePump1 = now;
        digitalWrite(PUMP1_PIN, pump1State ? LOW : HIGH);
        Serial.print("[Control] Pompa 1: ");
        Serial.println(pump1State ? "ON" : "OFF");
      }
    }
    
    // Logika Pompa 2 (Air Keluar Reservoir)
    bool shouldPump2On = !pump1State;
    if (shouldPump2On != pump2State) {
      if ((now - lastChangePump2) > MIN_CHANGE_INTERVAL) {
        pump2State = shouldPump2On;
        lastChangePump2 = now;
        digitalWrite(PUMP2_PIN, pump2State ? LOW : HIGH);
        Serial.print("[Control] Pompa 2: ");
        Serial.println(pump2State ? "ON" : "OFF");
      }
    }
    
    // Logika Pompa 3 (Sumur) dengan batas histeresis baru:
    // Hidupkan pompa jika ketinggian air sumur kurang dari 1 cm,
    // dan matikan pompa jika ketinggian air sumur mencapai atau melebihi 4 cm.
    /*
    bool shouldPump3On = pump3State;
    if (g_reservoirLevel2 < WATER_LEVEL3_ON_THRESHOLD) {
      shouldPump3On = true;
    } else if (g_reservoirLevel2 >= WATER_LEVEL3_OFF_THRESHOLD) {
      shouldPump3On = false;
    }
    
    if (shouldPump3On != pump3State) {
      if ((now - lastChangePump3) > MIN_CHANGE_INTERVAL) {
        pump3State = shouldPump3On;
        lastChangePump3 = now;
        digitalWrite(PUMP3_PIN, pump3State ? LOW : HIGH);
        Serial.print("[Control] Pompa 3: ");
        Serial.println(pump3State ? "ON" : "OFF");
      }
    }*/
    
    vTaskDelay(xDelay);
  }
}
