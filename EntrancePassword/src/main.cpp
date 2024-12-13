#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>
#include <ESP32Servo.h>
#include <esp_http_server.h>
#include "FS.h"

// Pin definitions
#define RX_PIN 16
#define TX_PIN 17
#define RED_PIN 14
#define GREEN_PIN 27
#define BLUE_PIN 26
#define SERVO_PIN 25
#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER_PIN 12

// Network and hub configuration
const char* ssid_primary = "DIGI-27Xy";
const char* password_primary = "3CfkabA5aa";
const char* ssid_secondary = "MobileRouter";
const char* password_secondary = "MobilePassword";

const char* hub_primary = "192.168.1.100"; // Bound to primary network
const char* hub_secondary = "192.168.1.43"; // Bound to secondary network
const char* PORT = "5000";
String hub_address = "";
const char* board_name = "FrontDoorESP32";
bool alarmActivated = false;

// Fingerprint sensor
Adafruit_Fingerprint finger(&Serial2);

// Servo motor
Servo myServo;

// Variables for fingerprint waiting mechanism
unsigned long detectionStartTime = 0;
unsigned long presenceEndTime = 0;
bool personDetected = false;
bool waitingForFingerprint = false;
unsigned long fingerprintStartTime = 0;

// Function to test hub connectivity
bool testHubConnection(const char* hub_ip) {
    HTTPClient http;
    String test_url = String("http://") + hub_ip + ":" + PORT + "/test";
    http.begin(test_url);
    int httpCode = http.GET();
    http.end();
    return (httpCode == 200);
}

// Register the board with the hub
void registerBoard() {
    if (hub_address == "") return;
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

// Attempt to connect to Wi-Fi
bool connectToWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(ssid);

    int retries = 20; // Retry limit
    while (WiFi.status() != WL_CONNECTED && retries > 0) {
        delay(500);
        Serial.print(".");
        retries--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to Wi-Fi: " + String(ssid));
        Serial.println("IP Address: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\nFailed to connect to Wi-Fi: " + String(ssid));
        return false;
    }
}

// Connect to the appropriate Wi-Fi and assign hub address
void setupWiFiAndHub() {
    if (connectToWiFi(ssid_primary, password_primary)) {
        hub_address = hub_primary;
    } else if (connectToWiFi(ssid_secondary, password_secondary)) {
        hub_address = hub_secondary;
    } else {
        Serial.println("Failed to connect to any Wi-Fi network. Halting...");
        while (true); // Halt execution if no Wi-Fi connection
    }
}

// Function to send proximity event
void sendProximityEvent(int distance) {
    if (hub_address == "") return;
    HTTPClient http;
    http.begin(String("http://") + hub_address + ":" + PORT + "/proximity_event");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"name\":\"FrontDoorESP32\",\"distance\":" + String(distance) + "}";
    int httpCode = http.POST(payload);
    if (httpCode == 200) {
        Serial.println("Proximity event sent successfully.");
    } else {
        Serial.println("Failed to send proximity event. HTTP code: " + String(httpCode));
    }
    http.end();
}

// Function to send fingerprint result
void sendFingerprintResult(String status, int id = -1) {
    if (hub_address == "") return;
    HTTPClient http;
    http.begin(String("http://") + hub_address + ":" + PORT + "/fingerprint_result");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"name\":\"FrontDoorESP32\",\"status\":\"" + status + "\"";
    if (id != -1) {
        payload += ",\"id\":" + String(id);
    }
    payload += "}";

    int httpCode = http.POST(payload);
    if (httpCode == 200) {
        Serial.println("Fingerprint result sent successfully.");
    } else {
        Serial.println("Failed to send fingerprint result. HTTP code: " + String(httpCode));
    }
    http.end();
}

// Ultrasonic sensor
long getDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    long distance = duration * 0.034 / 2; // Convert duration to distance
    return distance;
}

// RGB LED
void setRGBColor(int red, int green, int blue) {
    analogWrite(RED_PIN, red);
    analogWrite(GREEN_PIN, green);
    analogWrite(BLUE_PIN, blue);
}

// Servo control
void rotateServo(int durationMillis, int direction) {
    if (direction == 0) {
        myServo.write(0);
    } else if (direction == 1) {
        myServo.write(180);
    }
    delay(durationMillis);
    myServo.write(90);
}

// Fingerprint handling
int getFingerprintID() {
    int p = finger.getImage();
    if (p != FINGERPRINT_OK) return -1;

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) return -1;

    p = finger.fingerFastSearch();
    if (p != FINGERPRINT_OK) return -1;

    return finger.fingerID;
}

// Buzzer
void buzz(int duration) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
}

void handleSuccess() {
    setRGBColor(0, 255, 0);
    buzz(200);
    rotateServo(2000, 1);
    delay(2000);
    rotateServo(2000, 0);
    setRGBColor(0, 0, 0);
}

void handleFailure() {
    alarmActivated = true;
    for (int i = 0; i < 3; i++) {
        setRGBColor(255, 0, 0);
        buzz(200);
        delay(500);
        setRGBColor(0, 0, 0);
        delay(500);
    }
}

// Handler for activating the alarm

esp_err_t activate_alarm_handler(httpd_req_t *req) {
    // Activate the alarm
    alarmActivated = true;

    // Turn on the buzzer and RGB LED as alarm indicators
    digitalWrite(BUZZER_PIN, HIGH);
    setRGBColor(255, 0, 0); // Red color for active alarm

    Serial.println("Alarm activated!");

    // Send response
    const char *resp = "Alarm activated!";
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

// Handler for deactivating the alarm
esp_err_t deactivate_alarm_handler(httpd_req_t *req) {
    // Deactivate the alarm
    alarmActivated = false;

    // Turn off the buzzer and RGB LED
    digitalWrite(BUZZER_PIN, LOW);
    setRGBColor(0, 0, 0); // Turn off the LED

    Serial.println("Alarm deactivated!");

    // Send response
    const char *resp = "Alarm deactivated!";
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

// Function to start the HTTP server and register routes
void startServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // URI for activating the alarm
    httpd_uri_t activate_alarm = {
        .uri = "/activate_alarm",
        .method = HTTP_POST,
        .handler = activate_alarm_handler,
        .user_ctx = NULL
    };

    // URI for deactivating the alarm
    httpd_uri_t deactivate_alarm = {
        .uri = "/deactivate_alarm",
        .method = HTTP_POST,
        .handler = deactivate_alarm_handler,
        .user_ctx = NULL
    };

    // Start the HTTP server
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register the routes
        httpd_register_uri_handler(server, &activate_alarm);
        httpd_register_uri_handler(server, &deactivate_alarm);

        Serial.println("HTTP server started and routes registered.");
    } else {
        Serial.println("Failed to start the HTTP server.");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Initializing system...");

    // RGB LED setup
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);

    // Servo setup
    myServo.attach(SERVO_PIN);
    myServo.write(90);

    // Ultrasonic sensor setup
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Buzzer setup
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // Fingerprint sensor setup
    Serial2.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
    finger.begin(57600);

    if (finger.verifyPassword()) {
        Serial.println("Fingerprint sensor detected successfully.");
        setRGBColor(0, 0, 255);
        delay(2000);
        setRGBColor(0, 0, 0);
    } else {
        Serial.println("Fingerprint sensor not found. Check wiring.");
        setRGBColor(255, 0, 0);
        while (1);
    }

    // Connect to Wi-Fi and set hub address
    setupWiFiAndHub();

    registerBoard();

    // Start the HTTP server
    startServer();
}

void loop() {
    long distance = getDistance();
    unsigned long currentMillis = millis();

    // Check for presence
    if (distance > 0 && distance < 25) {
        if (!personDetected) {
            detectionStartTime = currentMillis;
            personDetected = true;
            Serial.println("Presence detected. Monitoring...");
            setRGBColor(255, 255, 0); // Yellow: Detecting movement
        }

        // Confirm presence if within range for 2 seconds
        if (currentMillis - detectionStartTime >= 2000 && !waitingForFingerprint) {
            Serial.println("Confirmed presence. Waiting for fingerprint...");
            setRGBColor(0, 0, 255); // Blue: Waiting for fingerprint
            waitingForFingerprint = true;
            fingerprintStartTime = currentMillis;
        }

        presenceEndTime = currentMillis; // Update the presence end time
    } else if (personDetected) {
        // Reset if no presence for 5 seconds
        if (currentMillis - presenceEndTime >= 5000) {
            Serial.println("No presence detected. Resetting...");
            personDetected = false;
            waitingForFingerprint = false;
            setRGBColor(0, 0, 0); // Turn off LED
        }
    }

    // Handle fingerprint verification
    if (waitingForFingerprint) {
        if (currentMillis - fingerprintStartTime <= 20000) {
            int fingerprintID = getFingerprintID();
            if (fingerprintID != -1) {
                Serial.print("Fingerprint matched! ID: ");
                Serial.println(fingerprintID);
                setRGBColor(0, 255, 0); // Green: Access granted
                handleSuccess();
                personDetected = false;
                waitingForFingerprint = false;
                setRGBColor(0, 0, 0); // Turn off LED
            }
        } else {
            Serial.println("Fingerprint not recognized within 20 seconds.");
            setRGBColor(255, 0, 0); // Red: Access denied
            handleFailure();
            waitingForFingerprint = false;
            personDetected = false;
            setRGBColor(0, 0, 0); // Turn off LED
        }
    }
}


