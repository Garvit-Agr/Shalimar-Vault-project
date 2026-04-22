#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>          
#include "MAX30105.h"      
#include <ElegantOTA.h>    // FEATURE: Local Web OTA
#include "mbedtls/md.h"    // FEATURE: SHA256 Cryptographic Hashing

// Include our HTML files from the separate tabs
#include "client_html.h"
#include "login_html.h"
#include "manager_html.h"

// --- PINS ---
#define IR_PIN_1 14  
#define IR_PIN_2 27  
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LDR_PIN 32  
#define RADAR_SERVO_PIN 12      
#define SAFE_SERVO_PIN 13 
#define LED_G_PIN 19 
#define LED_R_PIN 26 
#define SDA_PIN 21   
#define SCL_PIN 22   

// --- SETTINGS ---
const char* WIFI_SSID = "Vault_Secure_Net";
const int DISTANCE_THRESHOLD = 15;      
const unsigned long SESSION_DURATION = 30000; // 30s Demo Time
const unsigned long EGRESS_DURATION = 10000;  // 10s exit timer

// --- PRE-COMPUTED SHA256 HASHES ---
// "vault123" = 83e580e0fb9d08e5e04ccbd2b21c4b79b5025cd0c8db4da65b82bfdf3a67732e
// "open123"  = 64da8a211755ee694939a3f2b7a421b8b8c56e3bd6bb5c68b7d41fbd6d38e210
String MANAGER_HASH = "";
String CLIENT_HASH  = "";

AsyncWebServer server(80);
Servo radarServo; 
Servo safeServo; 
MAX30105 particleSensor; 

// --- RADAR SWEEP VARIABLES ---
int minAngle = 45;
int maxAngle = 135;
int radarAngle = minAngle;
int sweepStep = 5;
unsigned long lastSweepTime = 0;

// --- STATE GLOBALS (FIXED: Marked 'volatile' for Dual-Core Safety) ---
volatile int occupancy = 0;
volatile bool isGateUnlocked = false;
volatile unsigned long unlockTimer = 0;
volatile int doorState = 0; 
volatile unsigned long doorTimeout = 0;
volatile bool prevIR1 = HIGH, prevIR2 = HIGH;  // <-- PUT THIS LINE BACK IN!

volatile bool sessionActive = false;
volatile bool requestPending = false;
volatile bool gateBreach = false;
volatile bool vaultBreach = false;
volatile bool hardwareLockdown = false;
volatile unsigned long sessionTimer = 0; 
int currentDistance = 0, currentLight = 1; 

// Egress Variables
volatile bool isEgressing = false;
volatile unsigned long egressTimer = 0;

// --- FEATURE: SHA256 Hashing Function ---
String getSHA256(String payload) {
  byte shaResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) payload.c_str(), payload.length());
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);

  String hashStr = "";
  for(int i= 0; i< 32; i++) {
    char str[3];
    sprintf(str, "%02x", (int)shaResult[i]);
    hashStr += str;
  }
  return hashStr;
}

void setup() {
  Serial.begin(115200);
  MANAGER_HASH = getSHA256("vault123");
  CLIENT_HASH = getSHA256("open123");
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

  // Optimized Pulse Sensor Parameters
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("[AUDIT ERROR] Pulse Sensor NOT FOUND! Check wiring.");
  } else {
    particleSensor.setup(60, 4, 2, 100, 411, 4096);
    particleSensor.setPulseAmplitudeRed(0x3F); 
    particleSensor.setPulseAmplitudeGreen(0);
  }

  // --- FEATURE: WPA2 Network Security ---
  Serial.println("\n[AUDIT INFO] Starting Vault Wi-Fi Network (WPA2 Encryption Enabled)...");
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  
  // WPA2 Password implemented (Counters Mirai open-network threat)
  WiFi.softAP(WIFI_SSID, "VaultSecure2026!");

  Serial.println("\n==================================================");
  Serial.println("[AUDIT INFO] VAULT SYSTEM ONLINE & SECURED");
  Serial.print("1. Connect to Wi-Fi: "); Serial.println(WIFI_SSID);
  Serial.println("   Password: VaultSecure2026!");
  Serial.println("2. User Dashboard: http://192.168.4.1");
  Serial.println("3. OTA Updates: http://192.168.4.1/update");
  Serial.println("==================================================\n");
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", client_html); });
  server.on("/client", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", client_html); });
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", login_html); });
  server.on("/manager", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", manager_html); });
  
  // --- FEATURE: Brute Force Lockout & SHA256 Verification ---
  static int failedLoginAttempts = 0;

  server.on("/auth", HTTP_POST, [](AsyncWebServerRequest *req){
    String u = "", p = "";
    if(req->hasParam("user", true)) u = req->getParam("user", true)->value();
    if(req->hasParam("pass", true)) p = req->getParam("pass", true)->value();
    
    // Hash password and compare securely
    if(u == "admin" && getSHA256(p) == MANAGER_HASH){ 
      failedLoginAttempts = 0; // Reset on success
      Serial.println("[AUDIT INFO] Manager authentication SUCCESS.");
      req->send(200, "application/json", "{\"ok\":true}"); 
    } 
    else { 
      failedLoginAttempts++;
      Serial.printf("[AUDIT WARN] Failed login attempt %d/3 from IP: %s\n", failedLoginAttempts, req->client()->remoteIP().toString().c_str());
      
      if(failedLoginAttempts >= 3) {
         hardwareLockdown = true;
         Serial.println("[SECURITY ALERT] BRUTE FORCE DETECTED. SYSTEM IN LOCKDOWN.");
      }
      req->send(200, "application/json", "{\"ok\":false}"); 
    }
  });

  server.on("/request", HTTP_POST, [](AsyncWebServerRequest *req){
    if (sessionActive) { req->send(403, "text/plain", "Occupied"); return; }
    if (requestPending) { req->send(403, "text/plain", "Pending"); return; }
    if (hardwareLockdown) { req->send(403, "text/plain", "Locked"); return; }
    
    // Hash password and compare securely
    if(req->hasParam("pass", true) && getSHA256(req->getParam("pass", true)->value()) == CLIENT_HASH){
      requestPending = true; 
      Serial.println("[AUDIT INFO] Client access request submitted.");
      req->send(200, "text/plain", "OK");
    } else { 
      Serial.println("[AUDIT WARN] Client provided invalid access PIN.");
      req->send(403, "text/plain", "Denied"); 
    }
  });

  server.on("/approve", HTTP_GET, [](AsyncWebServerRequest *req){
    if (hardwareLockdown) { req->send(403, "text/plain", "DENIED: LOCKDOWN"); return; }
    
    sessionTimer = millis(); 
    isEgressing = false; 
    requestPending = false; 
    sessionActive = true; 
    Serial.println("[AUDIT INFO] Manager APPROVED vault access.");
    req->send(200, "text/plain", "Approved");
  });

  server.on("/deny", HTTP_GET, [](AsyncWebServerRequest *req){ 
    requestPending = false; 
    Serial.println("[AUDIT INFO] Manager DENIED vault access.");
    req->send(200, "text/plain", "Denied"); 
  });

  server.on("/end_session", HTTP_POST, [](AsyncWebServerRequest *req){
    if (sessionActive) {
      sessionActive = false; 
      isEgressing = true; 
      egressTimer = millis(); 
      Serial.println("[AUDIT INFO] Client initiated SECURE VAULT sequence. Egress started.");
      req->send(200, "text/plain", "Safe Secured");
    } else { req->send(403, "text/plain", "Unauthorized"); }
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *req){
    sessionActive = false; requestPending = false; isEgressing = false;
    gateBreach = false; vaultBreach = false; failedLoginAttempts = 0;
    if (occupancy == 0) hardwareLockdown = false; 
    Serial.println("[AUDIT INFO] System MASTER RESET triggered by manager.");
    req->send(200, "text/plain", "Reset");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *req){
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
    json += "\"sessionSecs\":" + String(sessionSecs) + ","; 
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

  // FEATURE: OTA Updates
  ElegantOTA.begin(&server);

  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // Handle OTA Process
  ElegantOTA.loop();

  // --- BIOMETRIC SENSOR (FINGER PRESENCE DETECTION) ---
  static unsigned long fingerDetectTime = 0;
  static bool isFingerOnSensor = false;
  
  long irValue = particleSensor.getIR();
  
  if (irValue > 50000) { 
    if (!isFingerOnSensor) {
      isFingerOnSensor = true;
      fingerDetectTime = currentMillis;
      Serial.println("[AUDIT INFO] ⏳ Scanning Biometric... Please hold finger.");
    } 
    else if (currentMillis - fingerDetectTime > 2500) { 
      if (hardwareLockdown) {
        Serial.println("[SECURITY ALERT] 🛑 BIOMETRIC DENIED: SYSTEM IN LOCKDOWN");
        fingerDetectTime = currentMillis + 5000; 
      } else if (sessionActive) { 
        Serial.println("[SECURITY ALERT] 🛑 BIOMETRIC DENIED: Vault Session Active");
        fingerDetectTime = currentMillis + 5000; 
      } else if (!isGateUnlocked && doorState == 0) { 
        isGateUnlocked = true;
        unlockTimer = currentMillis; 
        Serial.println("[AUDIT INFO] ✅ [GATE] Unlocked via Biometric Scanner.");
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
        Serial.println("[SECURITY ALERT] 🛑 KEYBOARD DENIED: SYSTEM IN LOCKDOWN");
      }
      else if (sessionActive) { 
        Serial.println("[SECURITY ALERT] 🛑 KEYBOARD DENIED: Vault Session Active");
      }
      else { 
        isGateUnlocked = true;
        unlockTimer = currentMillis; 
        Serial.println("[AUDIT INFO] ✅ [GATE] Unlocked via Keyboard Override.");
      }
    }
  }
  
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
 if (sessionActive && (millis() - sessionTimer > SESSION_DURATION)) {
    sessionActive = false;
    isEgressing = true;
    egressTimer = millis();
    Serial.println("[AUDIT INFO] Session time expired. Auto-securing safe.");
  }
  if (isEgressing && (millis() - egressTimer > EGRESS_DURATION)) {
    isEgressing = false;
  }

  // --- FEATURE: Input Validation & Physical Access Logic ---
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
        Serial.println("[SECURITY ALERT] 🚨 TAILGATING DETECTED! Perimeter breached.");
      } else { 
        Serial.println("[AUDIT INFO] [GATE] Authorized Entry."); 
      }
      isGateUnlocked = false;
      doorState = 0;
    } else if (currentMillis - doorTimeout > 2000) doorState = 0;
  } else if (doorState == 2) { 
    if (currIR1 == LOW && prevIR1 == HIGH) { 
      if (occupancy > 0) occupancy--;
      doorState = 0; 
      Serial.println("[AUDIT INFO] [GATE] Person Exited.");
      if (occupancy == 0) { 
        gateBreach = false;
        vaultBreach = false; 
        sessionActive = false; 
        isEgressing = false; 
        hardwareLockdown = false; 
        Serial.println("[AUDIT INFO] [SYSTEM] Room is empty. Alarms AUTO-CLEARED.");
      }
    } else if (currentMillis - doorTimeout > 2000) doorState = 0; 
  }
  prevIR1 = currIR1;
  prevIR2 = currIR2;

  // --- SAFE SERVO LOGIC ---
  static bool isSafeCurrentlyOpen = false;
  bool shouldSafeBeOpen = (sessionActive && !hardwareLockdown && !gateBreach && !vaultBreach);

  if (shouldSafeBeOpen && !isSafeCurrentlyOpen) {
    safeServo.write(90);
    isSafeCurrentlyOpen = true;
    Serial.println("[AUDIT INFO] 🔓 SAFE UNLOCKED: Servo moved to 90");
  } 
  else if (!shouldSafeBeOpen && isSafeCurrentlyOpen) {
    safeServo.write(0);
    isSafeCurrentlyOpen = false;
    Serial.println("[AUDIT INFO] 🔒 SAFE LOCKED: Servo moved to 0");
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
        Serial.println("[SECURITY ALERT] 🚨 VAULT BREACH DETECTED! Proximity/Light sensor tripped."); 
      }
    }
  } else {
    vaultBreach = false;
  }
}