#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>          // ADDED: For I2C
#include "MAX30105.h"      // ADDED: Pulse Sensor Library

// Include our HTML files from the separate tabs
#include "client_html.h"
#include "login_html.h"
#include "manager_html.h"

// --- PINS ---
#define IR_PIN_1 14  
#define IR_PIN_2 27  
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LDR_PIN 32  // Digital LDR (1 = Dark, 0 = Light)
#define RADAR_SERVO_PIN 12      
#define SAFE_SERVO_PIN 13 
#define LED_G_PIN 19 // ADDED: Gate Green LED
#define LED_R_PIN 26 // ADDED: Gate Red LED
#define SDA_PIN 21   // ADDED: Pulse Sensor SDA
#define SCL_PIN 22   // ADDED: Pulse Sensor SCL

// --- SETTINGS ---
const char* WIFI_SSID = "Vault_Secure_Net";
const int DISTANCE_THRESHOLD = 15;      
const unsigned long SESSION_DURATION = 30000; // 30s Demo Time
const unsigned long EGRESS_DURATION = 10000;  // 10s exit timer

AsyncWebServer server(80);
Servo radarServo; 
Servo safeServo; 
MAX30105 particleSensor; // ADDED: Pulse Sensor Object

// --- RADAR SWEEP VARIABLES ---
int minAngle = 45;
int maxAngle = 135;
int radarAngle = minAngle;
int sweepStep = 5;
unsigned long lastSweepTime = 0;

// --- STATE GLOBALS ---
int occupancy = 0;
bool isGateUnlocked = false;
unsigned long unlockTimer = 0;
int doorState = 0; 
unsigned long doorTimeout = 0;
bool prevIR1 = HIGH, prevIR2 = HIGH;

bool sessionActive = false;
bool requestPending = false;
bool gateBreach = false;
bool vaultBreach = false;
bool hardwareLockdown = false;
unsigned long sessionTimer = 0;
int currentDistance = 0, currentLight = 1; 

// Egress Variables
bool isEgressing = false;
unsigned long egressTimer = 0;

void setup() {
  Serial.begin(115200);
  pinMode(IR_PIN_1, INPUT); pinMode(IR_PIN_2, INPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(LDR_PIN, INPUT_PULLUP);
  pinMode(LED_G_PIN, OUTPUT); pinMode(LED_R_PIN, OUTPUT);

  // Initialize and attach the servos
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  
  radarServo.setPeriodHertz(50);
  radarServo.attach(RADAR_SERVO_PIN, 500, 2400);
  radarServo.write(radarAngle); 
  
  safeServo.setPeriodHertz(50);
  safeServo.attach(SAFE_SERVO_PIN, 500, 2400);
  safeServo.write(0);
  
  // Start with Red LED on
  digitalWrite(LED_G_PIN, LOW);
  digitalWrite(LED_R_PIN, HIGH);

  // Initialize Pulse Sensor
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("⚠️ Pulse Sensor NOT FOUND! Check wiring.");
  } else {
    particleSensor.setup(); 
    particleSensor.setPulseAmplitudeRed(0x0A); // Low power to just detect finger presence
    particleSensor.setPulseAmplitudeGreen(0);  // Turn off green LED
  }

  // --- ACCESS POINT SETUP ---
  Serial.println("\nStarting Vault Wi-Fi Network...");
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(WIFI_SSID, NULL);

  Serial.println("\n--- VAULT SYSTEM ONLINE ---");
  Serial.print("1. Connect your phone to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  Serial.println("2. Open your Kodular App or go to: http://192.168.4.1");
  Serial.println("--------------------------------------------------");
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", client_html); });
  server.on("/client", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", client_html); });
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", login_html); });
  server.on("/manager", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", manager_html); });
  server.on("/auth", HTTP_POST, [](AsyncWebServerRequest *req){
    String u = "", p = "";
    if(req->hasParam("user", true)) u = req->getParam("user", true)->value();
    if(req->hasParam("pass", true)) p = req->getParam("pass", true)->value();
    if(u == "admin" && p == "vault123"){ req->send(200, "application/json", "{\"ok\":true}"); } 
    else { req->send(200, "application/json", "{\"ok\":false}"); }
  });

  server.on("/request", HTTP_POST, [](AsyncWebServerRequest *req){
    if (sessionActive) { req->send(403, "text/plain", "Occupied"); return; }
    if (requestPending) { req->send(403, "text/plain", "Pending"); return; }
    if (hardwareLockdown) { req->send(403, "text/plain", "Locked"); return; }
    
    if(req->hasParam("pass", true) && req->getParam("pass", true)->value() == "open123"){
      requestPending = true; req->send(200, "text/plain", "OK");
    } else { req->send(403, "text/plain", "Denied"); }
  });

  server.on("/approve", HTTP_GET, [](AsyncWebServerRequest *req){
    if (hardwareLockdown) { req->send(403, "text/plain", "DENIED: LOCKDOWN"); return; }
    sessionActive = true; requestPending = false; sessionTimer = millis(); 
    isEgressing = false; 
    req->send(200, "text/plain", "Approved");
  });

  server.on("/deny", HTTP_GET, [](AsyncWebServerRequest *req){ requestPending = false; req->send(200, "text/plain", "Denied"); });

  server.on("/end_session", HTTP_POST, [](AsyncWebServerRequest *req){
    if (sessionActive) {
      sessionActive = false; 
      isEgressing = true; 
      egressTimer = millis(); 
      req->send(200, "text/plain", "Safe Secured");
    } else { req->send(403, "text/plain", "Unauthorized"); }
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *req){
    sessionActive = false; requestPending = false; isEgressing = false;
    gateBreach = false; vaultBreach = false; 
    if (occupancy == 0) hardwareLockdown = false; 
    req->send(200, "text/plain", "Reset");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *req){
    // FIX: Properly calculate sessionSecs so the UI timer doesn't say "NaN"
    unsigned long sessionSecs = 0;
    if (sessionActive) sessionSecs = (millis() - sessionTimer) < SESSION_DURATION ? (SESSION_DURATION - (millis() - sessionTimer)) / 1000 : 0;
    
    unsigned long egressSecs = 0;
    if (isEgressing) egressSecs = (millis() - egressTimer) < EGRESS_DURATION ? (EGRESS_DURATION - (millis() - egressTimer)) / 1000 : 0;

    String json = "{";
    json += "\"distance\":" + String(currentDistance) + ",";
    json += "\"light\":" + String(currentLight) + ",";
    json += "\"people\":" + String(occupancy) + ",";
    json += "\"gateOpen\":" + String(isGateUnlocked ? "true" : "false") + ",";
    json += "\"sessionActive\":" + String(sessionActive ? "true" : "false") + ",";
    json += "\"sessionSecs\":" + String(sessionSecs) + ","; // FIXED: Now sending backend timer
    json += "\"requestPending\":" + String(requestPending ? "true" : "false") + ",";
    json += "\"gateBreach\":" + String(gateBreach ? "true" : "false") + ",";
    json += "\"vaultBreach\":" + String(vaultBreach ? "true" : "false") + ",";
    json += "\"systemBreach\":" + String((gateBreach || vaultBreach) ? "true" : "false") + ",";
    json += "\"lockdown\":" + String(hardwareLockdown ? "true" : "false") + ",";
    json += "\"isEgressing\":" + String(isEgressing ? "true" : "false") + ",";
    json += "\"egressSecs\":" + String(egressSecs);
    json += "}";
    req->send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- BIOMETRIC SENSOR (FINGER PRESENCE DETECTION) ---
  static unsigned long fingerDetectTime = 0;
  static bool isFingerOnSensor = false;
  
  long irValue = particleSensor.getIR();
  
  if (irValue > 50000) { 
    if (!isFingerOnSensor) {
      isFingerOnSensor = true;
      fingerDetectTime = currentMillis;
      Serial.println("⏳ Scanning Biometric... Please hold finger.");
    } 
    // After holding for 2.5 seconds continuously
    else if (currentMillis - fingerDetectTime > 2500) { 
      if (hardwareLockdown) {
        Serial.println("🛑 DENIED: SYSTEM IN LOCKDOWN");
        fingerDetectTime = currentMillis + 5000; 
      } else if (sessionActive) { 
        Serial.println("🛑 DENIED: Vault Session Active");
        fingerDetectTime = currentMillis + 5000; 
      } else if (!isGateUnlocked && doorState == 0) { 
        isGateUnlocked = true;
        unlockTimer = currentMillis; 
        Serial.println("✅ [GATE] Unlocked via Biometric Scanner.");
        fingerDetectTime = currentMillis + 5000; 
      }
    }
  } else {
    isFingerOnSensor = false;
  }

  // 1. Biometric Simulation Backup (Keyboard Override)
  if (Serial.available() > 0) {
    char incoming = Serial.read();
    if (incoming == 'A' || incoming == 'a') { 
      if (hardwareLockdown) {
        Serial.println("🛑 DENIED: SYSTEM IN LOCKDOWN");
      }
      else if (sessionActive) { 
        Serial.println("🛑 DENIED: Vault Session Active");
      }
      else { 
        isGateUnlocked = true;
        unlockTimer = currentMillis; 
        Serial.println("[GATE] Unlocked via Keyboard.");
      }
    }
  }
  
  // Changed to 10000 for 10 second unlock
  if (isGateUnlocked && (currentMillis - unlockTimer > 10000)) isGateUnlocked = false;

  // LED LOGIC based on isGateUnlocked state
  if (isGateUnlocked) {
    digitalWrite(LED_G_PIN, HIGH);
    digitalWrite(LED_R_PIN, LOW);
  } else {
    digitalWrite(LED_G_PIN, LOW);
    digitalWrite(LED_R_PIN, HIGH);
  }

  // Transition logic for sessions ending naturally vs early exit
  if (sessionActive && (currentMillis - sessionTimer > SESSION_DURATION)) {
    sessionActive = false;
    isEgressing = true;
    egressTimer = currentMillis;
  }
  if (isEgressing && (currentMillis - egressTimer > EGRESS_DURATION)) {
    isEgressing = false;
  }

  // 2. IR Gate Tracking
  bool currIR1 = digitalRead(IR_PIN_1), currIR2 = digitalRead(IR_PIN_2);
  
  if (doorState == 0) {
    if (currIR1 == LOW && prevIR1 == HIGH) { 
      doorState = 1;
      doorTimeout = currentMillis; 
    } else if (currIR2 == LOW && prevIR2 == HIGH) { 
      doorState = 2;
      doorTimeout = currentMillis; 
    }
  } else if (doorState == 1) { 
    if (currIR2 == LOW && prevIR2 == HIGH) { 
      occupancy++;
      if (!isGateUnlocked) { 
        gateBreach = true; 
        hardwareLockdown = true; 
        Serial.println("🚨 TAILGATING DETECTED!");
      } else { 
        Serial.println("[GATE] Authorized Entry."); 
      }
      isGateUnlocked = false;
      doorState = 0;
    } else if (currentMillis - doorTimeout > 2000) doorState = 0;
  } else if (doorState == 2) { 
    if (currIR1 == LOW && prevIR1 == HIGH) { 
      if (occupancy > 0) occupancy--;
      doorState = 0; 
      Serial.println("[GATE] Person Exited.");
      if (occupancy == 0) { 
        gateBreach = false;
        vaultBreach = false; 
        sessionActive = false; 
        isEgressing = false; 
        hardwareLockdown = false; 
        Serial.println("[SYSTEM] Room is empty. Alarms AUTO-CLEARED.");
      }
    } else if (currentMillis - doorTimeout > 2000) doorState = 0; 
  }
  prevIR1 = currIR1;
  prevIR2 = currIR2;

  // --- SAFE SERVO LOGIC ---
  if (sessionActive && !hardwareLockdown && !gateBreach && !vaultBreach) {
    safeServo.write(90);
  } else {
    safeServo.write(0);
  }

  // 3. Vault Sensor Tracking & Radar Sweep
  if (currentMillis - lastSweepTime >= 100) {
    lastSweepTime = currentMillis;
    radarAngle += sweepStep;
    if (radarAngle >= maxAngle || radarAngle <= minAngle) sweepStep = -sweepStep;
    radarServo.write(radarAngle);

    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10); digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 15000); 
    currentDistance = (duration > 0) ? (duration * 0.0343) / 2.0 : 999;
    currentLight = digitalRead(LDR_PIN); 
  }

  static unsigned long lastDebugPrint = 0;
  if (currentMillis - lastDebugPrint > 1000) {
    Serial.print("[STATUS] Dist: "); Serial.print(currentDistance); 
    Serial.print("cm | LDR: "); Serial.print(currentLight);
    Serial.print(" | PPL: "); Serial.println(occupancy);
    lastDebugPrint = currentMillis;
  }

  // 4. Vault Alarm Logic
  if (!sessionActive && !isEgressing) {
    if ((currentDistance > 0 && currentDistance < DISTANCE_THRESHOLD) || (currentLight == 0)) {
      if (!vaultBreach) { 
        vaultBreach = true;
        hardwareLockdown = true; 
        Serial.println("🚨 VAULT BREACH DETECTED!"); 
      }
    }
  } else {
    vaultBreach = false;
  }
  delay(50); 
}