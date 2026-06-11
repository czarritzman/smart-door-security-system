#define BLYNK_TEMPLATE_ID "TMPL6vqukwSSC"
#define BLYNK_TEMPLATE_NAME "Smart door lock"
#define BLYNK_AUTH_TOKEN "R1BE5UAXzC59ZhgRJGZSqn_OzEFFJiRM"

#define BLYNK_PRINT Serial

#define SS_PIN 5
#define RST_PIN 22

#define GREEN_LED 13
#define RED_LED 12
#define BUZZER 14

#define VPIN_DOOR_CONTROL V0
#define VPIN_SECURITY_LED V3
#define VPIN_BUZZER_STATUS V4
#define VPIN_TERMINAL_LOG V5
#define VPIN_PIN_INPUT V8
#define VPIN_FAILED_ATTEMPTS V9

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <MFRC522.h>

char ssid[] = "20-3";
char pass[] = "Kampungbaru123";

// oneM2M Middleware URL
String oneM2MBaseURL = "http://192.168.0.45:5000";

MFRC522 rfid(SS_PIN, RST_PIN);

String allowedUID = "FE 6B 4B 06";

int correctPIN = 1234;
int enteredPIN = 0;
int failedAttempts = 0;
bool waitingForPIN = false;

bool authorizedAccess = false;
unsigned long accessStartTime = 0;
const unsigned long accessDuration = 5000;

WidgetLED securityLED(VPIN_SECURITY_LED);
WidgetTerminal terminal(VPIN_TERMINAL_LOG);

void addLog(String message) {
  Serial.println(message);
  terminal.println(message);
  terminal.flush();
}

void sendToOneM2M(String endpoint, String jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = oneM2MBaseURL + endpoint;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(3000);

    int httpResponseCode = http.POST(jsonData);

    Serial.print("oneM2M POST ");
    Serial.print(endpoint);
    Serial.print(" Response: ");
    Serial.println(httpResponseCode);

    http.end();
  } else {
    Serial.println("oneM2M Error: WiFi not connected");
  }
}

void sendAccessLog(String rfidUID, String rfidStatus, String pinStatus, String accessStatus, int attempts) {
  String jsonData = "{";
  jsonData += "\"rfid_uid\":\"" + rfidUID + "\",";
  jsonData += "\"rfid_status\":\"" + rfidStatus + "\",";
  jsonData += "\"pin_status\":\"" + pinStatus + "\",";
  jsonData += "\"access_status\":\"" + accessStatus + "\",";
  jsonData += "\"failed_attempts\":" + String(attempts);
  jsonData += "}";

  sendToOneM2M("/onem2m/access-log", jsonData);
}

void sendDoorStatus(String doorStatus) {
  String jsonData = "{";
  jsonData += "\"door_status\":\"" + doorStatus + "\"";
  jsonData += "}";

  sendToOneM2M("/onem2m/door-status", jsonData);
}

void sendSecurityAlert(String alertStatus, String reason, int attempts) {
  String jsonData = "{";
  jsonData += "\"alert_status\":\"" + alertStatus + "\",";
  jsonData += "\"reason\":\"" + reason + "\",";
  jsonData += "\"failed_attempts\":" + String(attempts);
  jsonData += "}";

  sendToOneM2M("/onem2m/security-alert", jsonData);
}

BLYNK_WRITE(VPIN_DOOR_CONTROL) {
  int value = param.asInt();

  if (value == 1) {
    authorizedAccess = true;
    accessStartTime = millis();

    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);

    Blynk.virtualWrite(VPIN_BUZZER_STATUS, "OFF");
    securityLED.on();

    addLog("Manual unlock activated from Blynk");

    sendDoorStatus("UNLOCKED");
    sendAccessLog("BLYNK_CONTROL", "MANUAL", "NOT_REQUIRED", "MANUAL_UNLOCK", failedAttempts);

  } else {
    authorizedAccess = false;
    waitingForPIN = false;

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);

    Blynk.virtualWrite(VPIN_BUZZER_STATUS, "OFF");
    securityLED.off();

    addLog("Door locked from Blynk");

    sendDoorStatus("LOCKED");
    sendAccessLog("BLYNK_CONTROL", "MANUAL", "NOT_REQUIRED", "MANUAL_LOCK", failedAttempts);
  }
}

BLYNK_WRITE(VPIN_PIN_INPUT) {
  enteredPIN = param.asInt();

  addLog("PIN Entered: " + String(enteredPIN));

  if (!waitingForPIN) {
    addLog("Scan authorized RFID first before entering PIN");
    addLog("--------------------------------");
    return;
  }

  if (enteredPIN == correctPIN) {
    addLog("PIN CORRECT - ACCESS GRANTED");

    authorizedAccess = true;
    waitingForPIN = false;
    accessStartTime = millis();

    failedAttempts = 0;

    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);

    Blynk.virtualWrite(VPIN_DOOR_CONTROL, 1);
    Blynk.virtualWrite(VPIN_BUZZER_STATUS, "OFF");
    Blynk.virtualWrite(VPIN_FAILED_ATTEMPTS, failedAttempts);

    securityLED.on();

    sendDoorStatus("UNLOCKED");
    sendAccessLog(allowedUID, "AUTHORIZED", "CORRECT", "ACCESS_GRANTED", failedAttempts);

  } else {
    failedAttempts++;

    addLog("WRONG PIN - ACCESS DENIED");
    addLog("Failed Attempts: " + String(failedAttempts));

    Blynk.virtualWrite(VPIN_FAILED_ATTEMPTS, failedAttempts);
    Blynk.virtualWrite(VPIN_BUZZER_STATUS, "ON");

    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER, HIGH);

    sendAccessLog(allowedUID, "AUTHORIZED", "WRONG", "ACCESS_DENIED", failedAttempts);

    delay(2000);

    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);

    Blynk.virtualWrite(VPIN_BUZZER_STATUS, "OFF");

    if (failedAttempts >= 3) {
      addLog("SECURITY ALERT! TOO MANY FAILED PIN ATTEMPTS!");

      Blynk.logEvent("unauthorized_card", "Too many wrong PIN attempts!");

      sendSecurityAlert("ALERT_TRIGGERED", "3 wrong PIN attempts", failedAttempts);
      sendDoorStatus("LOCKED");

      digitalWrite(BUZZER, HIGH);
      delay(5000);
      digitalWrite(BUZZER, LOW);

      failedAttempts = 0;
      waitingForPIN = false;

      Blynk.virtualWrite(VPIN_FAILED_ATTEMPTS, failedAttempts);
      Blynk.virtualWrite(VPIN_DOOR_CONTROL, 0);
      securityLED.off();

      addLog("PIN verification cancelled");
    }
  }

  addLog("--------------------------------");
}

void setup() {
  Serial.begin(115200);

  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Blynk.virtualWrite(VPIN_DOOR_CONTROL, 0);
  Blynk.virtualWrite(VPIN_BUZZER_STATUS, "OFF");
  Blynk.virtualWrite(VPIN_FAILED_ATTEMPTS, 0);
  Blynk.virtualWrite(VPIN_PIN_INPUT, 0);

  securityLED.off();

  addLog("================================");
  addLog("SMART DOOR SECURITY SYSTEM ONLINE");
  addLog("Security Feature: RFID + PIN enabled");
  addLog("oneM2M Middleware Integration enabled");
  addLog("Blue tag -> Enter PIN to unlock");
  addLog("White card -> Access Denied");
  addLog("Correct PIN: 1234");
  addLog("Tap RFID card/tag...");
  addLog("================================");

  sendDoorStatus("LOCKED");
}

void loop() {
  Blynk.run();

  if (authorizedAccess && millis() - accessStartTime > accessDuration) {
    authorizedAccess = false;
    waitingForPIN = false;

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER, LOW);

    Blynk.virtualWrite(VPIN_DOOR_CONTROL, 0);
    Blynk.virtualWrite(VPIN_BUZZER_STATUS, "OFF");
    securityLED.off();

    addLog("Access session expired");
    addLog("--------------------------------");

    sendDoorStatus("LOCKED");
    sendAccessLog("AUTO_LOCK", "SYSTEM", "NOT_REQUIRED", "ACCESS_SESSION_EXPIRED", failedAttempts);
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uid += "0";
    }

    uid += String(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1) {
      uid += " ";
    }
  }

  uid.toUpperCase();

  addLog("UID Detected: " + uid);

  if (uid == allowedUID) {
    addLog("RFID AUTHORIZED");
    addLog("Please enter PIN in Blynk");

    waitingForPIN = true;

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);

    Blynk.virtualWrite(VPIN_DOOR_CONTROL, 0);
    Blynk.virtualWrite(VPIN_BUZZER_STATUS, "OFF");

    securityLED.on();

    sendAccessLog(uid, "AUTHORIZED", "PENDING", "WAITING_FOR_PIN", failedAttempts);

  } else {
    addLog("ACCESS DENIED - Unauthorized Card");
    addLog("SECURITY ALERT!");

    failedAttempts++;
    Blynk.virtualWrite(VPIN_FAILED_ATTEMPTS, failedAttempts);

    waitingForPIN = false;
    authorizedAccess = false;

    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER, HIGH);

    Blynk.virtualWrite(VPIN_DOOR_CONTROL, 0);
    Blynk.virtualWrite(VPIN_BUZZER_STATUS, "ON");

    securityLED.on();

    Blynk.logEvent("unauthorized_card", "Unauthorized RFID card detected!");

    sendAccessLog(uid, "UNAUTHORIZED", "NOT_REQUIRED", "ACCESS_DENIED", failedAttempts);
    sendSecurityAlert("ALERT_TRIGGERED", "Unauthorized RFID card detected", failedAttempts);
    sendDoorStatus("LOCKED");

    delay(3000);

    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);

    Blynk.virtualWrite(VPIN_BUZZER_STATUS, "OFF");
    securityLED.off();
  }

  addLog("--------------------------------");

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(500);
}