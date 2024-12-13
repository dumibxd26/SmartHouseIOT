#include "Keypad/Keypad.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Define HC-SR04 pins
#define TRIG_PIN 18
#define ECHO_PIN 19

// Define Buzzer pin
#define BUZZER_PIN 33

// Keypad setup
const int numRows = 3;
const int numCols = 3;
const int rowPins[numRows] = {13, 12, 14}; // Row pins
const int colPins[numCols] = {27, 26, 25}; // Column pins
const char *keys[numRows] = {"123", "456", "789"}; // Keypad layout

// Network and hub configuration
const char* ssid_primary = "DIGI-27Xy";
const char* password_primary = "3CfkabA5aa";
const char* ssid_secondary = "MobileRouter";
const char* password_secondary = "MobilePassword";

Keypad keypad(numRows, numCols, rowPins, colPins, keys);

// Password variables
String enteredPassword = "";           // Stores entered password
const String correctPassword = "1523"; // Set correct password
char lastKeyPressed = '\0';            // To track debouncing
unsigned long lastTimeKeyPressed = 0;  // Timestamp for last key press

bool alarmActive = false;
unsigned long systemDisabledUntil = 0; // Timestamp to disable system for 60 seconds

// Function to measure distance with HC-SR04
long measureDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    long distance = duration * 0.034 / 2; // Convert to cm
    return distance;
}

// Function to start the alarm
void startAlarm() {
    if (!alarmActive) {
        alarmActive = true;
        Serial.println("Alarm triggered!");
        digitalWrite(BUZZER_PIN, LOW); // Turn on buzzer
    }
}

// Function to stop the alarm
void stopAlarm() {
    if (alarmActive) {
        alarmActive = false;
        Serial.println("Alarm deactivated. System will restart in 60 seconds.");
        digitalWrite(BUZZER_PIN, HIGH); // Turn off buzzer
        systemDisabledUntil = millis() + 60000; // Disable for 60 seconds
    }
}

// Handle password entry logic
void handlePasswordEntry(char key) {
    if (key == '9') { // Reset the password if '9' is pressed
        enteredPassword = "";
        Serial.println("Password reset.");
        return;
    }

    // Avoid recognizing the same keypress repeatedly
    if (key != lastKeyPressed) {
        lastKeyPressed = key;
        lastTimeKeyPressed = millis();

        // Add key to the entered password
        enteredPassword += key;
        Serial.println("Key pressed: " + String(key));
        Serial.println("Current Password: " + enteredPassword);

        // Check password length and validate
        if (enteredPassword.length() == 4) {
            if (enteredPassword == correctPassword) {
                stopAlarm(); // Deactivate the alarm
            } else {
                Serial.println("Incorrect password!");
            }
            enteredPassword = ""; // Reset the password for next attempt
        }
    }
}

const char* hub_primary = "192.168.1.100"; // Bound to primary network
const char* hub_secondary = "192.168.1.43"; // Bound to secondary network
const char* PORT = "8080";
String hub_address = "";

const char* board_name = "ProximityBoard"; // Board name

// Function to connect to WiFi
bool connectToWiFi(const char* ssid, const char* password) {
    Serial.println("Attempting to connect to WiFi: " + String(ssid));
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 10000; // 10 seconds timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi: " + String(ssid));
        Serial.println("IP Address: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\nFailed to connect to WiFi: " + String(ssid));
        return false;
    }
}

// Function to test hub connectivity
bool testHubConnection(const char* hub_ip) {
    HTTPClient http;
    String test_url = String("http://") + hub_ip + ":" + PORT + "/test";
    http.begin(test_url);
    int httpCode = http.GET();
    http.end();
    return (httpCode == 200);
}

// Function to decide the hub address
void decideHubAddress() {
    if (WiFi.SSID() == ssid_primary) {
        hub_address = hub_primary;
    } else if (WiFi.SSID() == ssid_secondary) {
        hub_address = hub_secondary;
    } else {
        hub_address = "";
    }

    if (!hub_address.isEmpty() && testHubConnection(hub_address.c_str())) {
        Serial.println("Hub connection successful: " + hub_address);
    } else {
        Serial.println("Failed to connect to hub. Check configuration.");
        hub_address = ""; // Clear hub address if connection fails
    }
}

// Function to register the board with the hub
void registerBoard() {
    if (hub_address.isEmpty()) return;

    HTTPClient http;
    http.begin(String("http://") + hub_address + ":" + PORT + "/register");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"name\":\"" + String(board_name) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    int httpCode = http.POST(payload);
    if (httpCode == 200) {
        Serial.println("Board registered successfully.");
    } else {
        Serial.println("Failed to register board. HTTP code: " + String(httpCode));
    }
    http.end();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Proximity Alarm System with Network Hub Integration");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, HIGH); // Ensure buzzer is off initially
    keypad.initialize();

    // Attempt to connect to primary network
    if (!connectToWiFi(ssid_primary, password_primary)) {
        // If primary network fails, try secondary network
        if (!connectToWiFi(ssid_secondary, password_secondary)) {
            Serial.println("Failed to connect to any network. System will restart.");
            ESP.restart();
        }
    }

    // Decide the hub address based on the connected network
    decideHubAddress();

    // Register the board with the hub if connected
    registerBoard();

}

// Main loop
void loop() {
    // Wait for 60 seconds if system is disabled
    if (millis() < systemDisabledUntil) {
        return;
    }

    // Measure distance
    long distance = measureDistance();

    // Trigger alarm if proximity is detected
    if (distance > 0 && distance < 25 && !alarmActive) {
        startAlarm();
    }

    // Handle keypad input
    char key = keypad.getKey();
    if (key != '\0' && millis() - lastTimeKeyPressed > 250) { // Debouncing
        handlePasswordEntry(key);
    }
}
