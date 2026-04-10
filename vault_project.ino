#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Include our HTML files from the separate tabs
#include "client_html.h"
#include "login_html.h"
#include "manager_html.h"

// --- PINS ---
#define IR_PIN_1 14  
#define IR_PIN_2 27  
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LDR_PIN 32        // Digital LDR (1 = Dark, 0 = Light)
#define LED_G_PIN 19      // Green LED (Gate Unlocked)
#define LED_R_PIN 21      // Red LED (Gate Locked / Alarm)

// --- SETTINGS ---
const char* WIFI_SSID = "Vault_Secure_Net";
const int DISTANCE_THRESHOLD = 15;      
const unsigned long SESSION_DURATION = 30000; // 30s Demo Time

AsyncWebServer server(80);

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

void setup() {
  Serial.begin(115200);
  pinMode(IR_PIN_1, INPUT); pinMode(IR_PIN_2, INPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(LDR_PIN, INPUT_PULLUP);
  pinMode(LED_G_PIN, OUTPUT); pinMode(LED_R_PIN, OUTPUT);

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
  Serial.println("2. Open your browser to: http://192.168.4.1");
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
    req->send(200, "text/plain", "Approved");
  });

  server.on("/deny", HTTP_GET, [](AsyncWebServerRequest *req){ requestPending = false; req->send(200, "text/plain", "Denied"); });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *req){
    sessionActive = false; requestPending = false; 
    gateBreach = false; vaultBreach = false; 
    // ZERO-TRUST CHECK: Only clear hardware lockdown if room is truly empty
    if (occupancy == 0) hardwareLockdown = false; 
    req->send(200, "text/plain", "Reset");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *req){
    String json = "{";
    json += "\"distance\":" + String(currentDistance) + ",";
    json += "\"light\":" + String(currentLight) + ",";
    json += "\"people\":" + String(occupancy) + ",";
    json += "\"gateOpen\":" + String(isGateUnlocked ? "true" : "false") + ",";
    json += "\"sessionActive\":" + String(sessionActive ? "true" : "false") + ",";
    json += "\"requestPending\":" + String(requestPending ? "true" : "false") + ",";
    json += "\"gateBreach\":" + String(gateBreach ? "true" : "false") + ",";
    json += "\"vaultBreach\":" + String(vaultBreach ? "true" : "false") + ",";
    json += "\"systemBreach\":" + String((gateBreach || vaultBreach) ? "true" : "false") + ",";
    json += "\"lockdown\":" + String(hardwareLockdown ? "true" : "false");
    json += "}";
    req->send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  // --- 1. SIMULATED BIOMETRICS (TEAM HEIST MODE) ---
  if (Serial.available() > 0) {
    char inChar = Serial.read();
    if (inChar == 'A' || inChar == 'a') {
      // Multiple entries allowed, but block if safe is open or alarms are ringing
      if (sessionActive || hardwareLockdown) {
        Serial.println("🛑 ACCESS DENIED: System is currently in LOCKDOWN or VAULT IN USE.");
      } else {
        isGateUnlocked = true; 
        unlockTimer = millis(); 
        Serial.println("[GATE] TEAM ENTRY UNLOCKED. Please enter.");
      }
    }
  }

  // --- 2. GATE LED INDICATORS ---
  if (isGateUnlocked) {
    digitalWrite(LED_G_PIN, HIGH);
    digitalWrite(LED_R_PIN, LOW);
  } else {
    digitalWrite(LED_G_PIN, LOW);
    digitalWrite(LED_R_PIN, HIGH);
  }
  
  if (isGateUnlocked && (millis() - unlockTimer > 5000)) isGateUnlocked = false;
  if (sessionActive && (millis() - sessionTimer > SESSION_DURATION)) sessionActive = false;

  // --- 3. IR LASER GATE TRACKING ---
  bool currIR1 = digitalRead(IR_PIN_1), currIR2 = digitalRead(IR_PIN_2);

  if (doorState == 0) {
    if (currIR1 == LOW && prevIR1 == HIGH) { doorState = 1; doorTimeout = millis(); } 
    else if (currIR2 == LOW && prevIR2 == HIGH) { doorState = 2; doorTimeout = millis(); }
  } else if (doorState == 1) { 
    if (currIR2 == LOW && prevIR2 == HIGH) { 
      occupancy++;
      if (!isGateUnlocked) { gateBreach = true; hardwareLockdown = true; Serial.println("🚨 TAILGATING DETECTED!"); }
      else { Serial.println("[GATE] Authorized Entry."); }
      isGateUnlocked = false;
      doorState = 0;
    } else if (millis() - doorTimeout > 2000) doorState = 0;
  } else if (doorState == 2) { 
    if (currIR1 == LOW && prevIR1 == HIGH) { 
      if (occupancy > 0) occupancy--;
      doorState = 0; 
      Serial.println("[GATE] Person Exited.");
      if (occupancy == 0) { 
        gateBreach = false; vaultBreach = false; sessionActive = false; hardwareLockdown = false; 
        Serial.println("[SYSTEM] Room is empty. Alarms AUTO-CLEARED."); 
      }
    } else if (millis() - doorTimeout > 2000) doorState = 0;
  }
  prevIR1 = currIR1; prevIR2 = currIR2;

  // --- 4. VAULT SENSOR TRACKING ---
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10); digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 25000); 
  currentDistance = (duration > 0) ? (duration * 0.0343) / 2.0 : 999;
  currentLight = digitalRead(LDR_PIN); 

  if (!sessionActive) {
    if ((currentDistance > 0 && currentDistance < DISTANCE_THRESHOLD) || (currentLight == 0)) {
      if (!vaultBreach) { vaultBreach = true; hardwareLockdown = true; Serial.println("🚨 VAULT BREACH DETECTED!"); }
    }
  } else {
    vaultBreach = false;
  }

  delay(50); 
}