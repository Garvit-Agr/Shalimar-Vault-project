#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <ESP32Servo.h>

// Include our 3 HTML tabs
#include "client_html.h"
#include "login_html.h"
#include "manager_html.h"

// --- PINS ---
#define IR_PIN_1 14       
#define IR_PIN_2 27       
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LDR_PIN 32        
#define LED_G_PIN 19      
#define LED_R_PIN 26      
#define SDA_PIN 21        
#define SCL_PIN 22        
#define RADAR_SERVO_PIN 12  // NEW: Replaced Gate Servo with Radar Servo
#define SAFE_SERVO_PIN 13 

// --- SETTINGS ---
const char* WIFI_SSID = "Vault_Secure_Net";
const int DISTANCE_THRESHOLD = 15;      
const unsigned long SESSION_DURATION = 30000; 
const unsigned long EGRESS_DURATION = 10000;  

AsyncWebServer server(80);
MAX30105 particleSensor;
Servo radarServo; // NEW: Radar motor
Servo safeServo;

// --- STATE GLOBALS ---
int occupancy = 0;
bool isGateUnlocked = false; // Kept intact for LEDs and UI logic
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
int pulseBeatCount = 0;

// --- RADAR GLOBALS ---
int radarAngle = 45; // Start angle
int sweepStep = 5;   // Degrees to move per tick

// --- ARCHITECTURE GLOBALS ---
String authorizedIP = "";      
bool isEgressing = false;
unsigned long egressTimer = 0;
bool gateOverride = false;     

void setup() {
  Serial.begin(115200);
  pinMode(IR_PIN_1, INPUT); pinMode(IR_PIN_2, INPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(LDR_PIN, INPUT_PULLUP);
  pinMode(LED_G_PIN, OUTPUT); pinMode(LED_R_PIN, OUTPUT);

  // --- SERVO SETUP ---
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  radarServo.setPeriodHertz(50);
  safeServo.setPeriodHertz(50);
  
  radarServo.attach(RADAR_SERVO_PIN, 500, 2400);
  safeServo.attach(SAFE_SERVO_PIN, 500, 2400);
  radarServo.write(radarAngle); 
  safeServo.write(0); 

  // --- PULSE SENSOR SETUP ---
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("⚠️ Pulse Sensor NOT FOUND! Check wiring.");
  } else {
    particleSensor.setup(); 
    particleSensor.setPulseAmplitudeRed(0x0A); 
    particleSensor.setPulseAmplitudeGreen(0);  
  }

  // --- ACCESS POINT SETUP (FIX 1: Password Added) ---
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(WIFI_SSID, "vault@iiit"); 
  
  Serial.println("\n--- VAULT SYSTEM ONLINE ---");

  // --- WEB ROUTES ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", client_html); });
  server.on("/client", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", client_html); });
  
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", login_html); });
  server.on("/auth", HTTP_POST, [](AsyncWebServerRequest *req){
    String u = "", p = "";
    if(req->hasParam("user", true)) u = req->getParam("user", true)->value();
    if(req->hasParam("pass", true)) p = req->getParam("pass", true)->value();
    if(u == "admin" && p == "vault123"){ req->send(200, "application/json", "{\"ok\":true}"); } 
    else { req->send(200, "application/json", "{\"ok\":false}"); }
  });

  server.on("/manager", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", manager_html); });

  // --- API ENDPOINTS ---
  server.on("/request", HTTP_POST, [](AsyncWebServerRequest *req){
    if (sessionActive || requestPending) { req->send(403, "text/plain", "Occupied"); return; }
    if (hardwareLockdown) { req->send(403, "text/plain", "Locked"); return; }
    
    if(req->hasParam("pass", true) && req->getParam("pass", true)->value() == "open123"){
      authorizedIP = req->client()->remoteIP().toString(); 
      requestPending = true; 
      req->send(200, "text/plain", "OK");
    } else { req->send(403, "text/plain", "Denied"); }
  });

  server.on("/approve", HTTP_GET, [](AsyncWebServerRequest *req){
    if (hardwareLockdown) { req->send(403, "text/plain", "DENIED: LOCKDOWN"); return; }
    sessionActive = true; requestPending = false; sessionTimer = millis(); 
    isEgressing = false; 
    req->send(200, "text/plain", "Approved");
  });

  server.on("/deny", HTTP_GET, [](AsyncWebServerRequest *req){ requestPending = false; req->send(200, "text/plain", "Denied"); });

  server.on("/end_session", HTTP_GET, [](AsyncWebServerRequest *req){
    if (req->client()->remoteIP().toString() == authorizedIP && sessionActive) {
      sessionActive = false; 
      isEgressing = true; 
      egressTimer = millis();
      req->send(200, "text/plain", "Vault Secured");
    } else { req->send(403, "text/plain", "Unauthorized"); }
  });

  server.on("/override_gate", HTTP_GET, [](AsyncWebServerRequest *req){
    gateOverride = true;
    isGateUnlocked = true;
    unlockTimer = millis();
    req->send(200, "text/plain", "Override Active");
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *req){
    sessionActive = false; requestPending = false; 
    gateBreach = false; vaultBreach = false; gateOverride = false;
    authorizedIP = "";
    if (occupancy == 0) hardwareLockdown = false; 
    req->send(200, "text/plain", "Reset");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *req){
    bool isAuth = (req->client()->remoteIP().toString() == authorizedIP);
    
    unsigned long sessionSecs = 0;
    if (sessionActive) {
      unsigned long elapsed = millis() - sessionTimer;
      sessionSecs = elapsed < SESSION_DURATION ? (SESSION_DURATION - elapsed) / 1000 : 0;
    }
    
    String json = "{";
    json += "\"distance\":" + String(currentDistance) + ",";
    json += "\"light\":" + String(currentLight) + ",";
    json += "\"people\":" + String(occupancy) + ",";
    json += "\"sessionActive\":" + String(sessionActive ? "true" : "false") + ",";
    json += "\"sessionSecs\":" + String(sessionSecs) + ",";
    json += "\"isAuthorizedUser\":" + String(isAuth ? "true" : "false") + ",";
    json += "\"requestPending\":" + String(requestPending ? "true" : "false") + ",";
    json += "\"lockdown\":" + String(hardwareLockdown ? "true" : "false") + ",";
    json += "\"gateBreach\":" + String(gateBreach ? "true" : "false") + ",";
    json += "\"vaultBreach\":" + String(vaultBreach ? "true" : "false") + ",";
    json += "\"gateOpen\":" + String(isGateUnlocked ? "true" : "false");
    json += "}";
    req->send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- 1. BIOMETRIC ENTRY ---
  if (!isGateUnlocked && !hardwareLockdown && doorState == 0) {
    long irValue = particleSensor.getIR(); 
    if (checkForBeat(irValue)) { pulseBeatCount++; }
    if (pulseBeatCount >= 3) {
      if (!sessionActive) {
        isGateUnlocked = true; unlockTimer = currentMillis; 
      }
      pulseBeatCount = 0;
    }
    if (irValue < 50000) pulseBeatCount = 0;
  }

  if (isGateUnlocked && (currentMillis - unlockTimer > 5000)) {
    isGateUnlocked = false; gateOverride = false;
  }

  // --- 2. LEDS & SAFE SERVO (Gate Servo Removed) ---
  // We keep the LED logic so users know when biometrics are accepted
  if ((isGateUnlocked && !hardwareLockdown) || gateOverride) {
    digitalWrite(LED_G_PIN, HIGH); digitalWrite(LED_R_PIN, LOW);
  } else {
    digitalWrite(LED_G_PIN, LOW); digitalWrite(LED_R_PIN, HIGH);
    isGateUnlocked = false; 
  }

  if (sessionActive && !hardwareLockdown) { safeServo.write(90); } 
  else { safeServo.write(0); }

  // --- 3. TIMERS ---
  if (sessionActive && (currentMillis - sessionTimer > SESSION_DURATION)) { 
    sessionActive = false; authorizedIP = "";
    isEgressing = true; egressTimer = currentMillis; 
  }
  
  if (isEgressing && (currentMillis - egressTimer > EGRESS_DURATION)) {
    isEgressing = false; 
  }

  // --- 4. IR LASER TRACKING ---
  bool currIR1 = digitalRead(IR_PIN_1), currIR2 = digitalRead(IR_PIN_2);

  if (doorState == 0) {
    if (currIR1 == LOW && prevIR1 == HIGH) { 
      doorState = 1; doorTimeout = currentMillis; 
    } else if (currIR2 == LOW && prevIR2 == HIGH && !hardwareLockdown) { 
      doorState = 2; doorTimeout = currentMillis; 
      isGateUnlocked = true; unlockTimer = currentMillis; 
    }
  } else if (doorState == 1) { 
    if (currIR2 == LOW && prevIR2 == HIGH) { 
      occupancy++;
      if (!isGateUnlocked) { gateBreach = true; hardwareLockdown = true; }
      isGateUnlocked = false; 
      doorState = 0;
    } else if (currentMillis - doorTimeout > 2000) doorState = 0;
  } else if (doorState == 2) { 
    if (currIR1 == LOW && prevIR1 == HIGH) { 
      if (occupancy > 0) occupancy--;
      isGateUnlocked = false; 
      doorState = 0; 
      if (occupancy == 0) { 
        gateBreach = false; vaultBreach = false; hardwareLockdown = false; 
        sessionActive = false; isEgressing = false; authorizedIP = "";
      }
    } else if (currentMillis - doorTimeout > 2000) doorState = 0;
  }
  prevIR1 = currIR1; prevIR2 = currIR2;

  // --- 5. VAULT SENSORS & RADAR SWEEP ---
  static unsigned long lastSensorRead = 0;
  if (currentMillis - lastSensorRead >= 100) {
    lastSensorRead = currentMillis;
    
    // NEW: Sweep the radar by 5 degrees
    radarAngle += sweepStep;
    if (radarAngle >= 135 || radarAngle <= 45) { // 90-degree field of view
      sweepStep = -sweepStep;
    }
    radarServo.write(radarAngle);

    // Read the ultrasonic sensor at this exact angle
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10); digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 25000); 
    currentDistance = (duration > 0) ? (duration * 0.0343) / 2.0 : 999;
    
    currentLight = digitalRead(LDR_PIN); 

    // Radar Alarm Check (Triggers if anything breaks 15cm at any sweeping angle)
    if (!sessionActive && !isEgressing) {
      if ((currentDistance > 0 && currentDistance < DISTANCE_THRESHOLD) || (currentLight == 0)) {
        if (!vaultBreach) { vaultBreach = true; hardwareLockdown = true; }
      }
    } else { vaultBreach = false; }
  }
}
