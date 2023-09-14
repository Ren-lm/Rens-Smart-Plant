/**
 * Plant Health Monitoring System
 * 
 * This program utilizes various sensors to monitor the health of a plant by measuring
 * parameters like moisture, light intensity, temperature, and humidity. It then calculates
 * the overall health of the plant based on these readings and displays them on an OLED screen.
 * Additionally, the system provides a web dashboard where one can view the same data in a user-friendly format.
 * 
 * Dependency Libraries:
 * - U8g2lib: For OLED display.
 * - Wire: For I2C communication.
 * - BH1750: Light sensor library.
 * - DHT: For DHT sensor (Temperature & Humidity).
 * - ESP8266WiFi: To connect to Wi-Fi.
 * - ESPAsyncTCP: Async TCP library for ESP8266.
 * - ESPAsyncWebSrv: Async Web Server library for ESP8266.
 */

#include <U8g2lib.h>
#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebSrv.h>

// OLED setup
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

// Initialize Light sensor
BH1750 lightMeter;

// Initialize Moisture sensor
#define MOISTURE_PIN A0
#define RELAY_PIN D7
float moisturePercentage = 0.0;

// Additional global variables for sensor readings
float lightPercentage = 0.0;
float temperature = 0.0;
float humidity = 0.0;

// Variables for plant health status
String plantType = "Default Plant";
String plantHealthStatus = "Good";
String plantHealthReason = "";

void updatePlantHealth() {
    plantHealthStatus = "Good"; 
    plantHealthReason = "";

    // Update health status based on different parameters
    if (temperature < 18 || temperature > 24) {
        plantHealthStatus = "At Risk";
        plantHealthReason += temperature < 18 ? "Temperature too low; " : "Temperature too high; ";
    }

    if (humidity < 40 || humidity > 65) {
        plantHealthStatus = "At Risk";
        plantHealthReason += humidity < 40 ? "Humidity too low; " : "Humidity too high; ";
    }

    if (lightPercentage < 60 || lightPercentage > 80) {
        plantHealthStatus = "At Risk";
        plantHealthReason += lightPercentage < 60 ? "Light too low; " : "Light too high; ";
    }

    if (moisturePercentage < 50 || moisturePercentage > 80) {
        plantHealthStatus = "At Risk";
        plantHealthReason += moisturePercentage < 50 ? "Moisture too low" : "Moisture too high";
    }
}

// DHT sensor setup
#define DHT_PIN D3
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// WiFi Credentials
const int MAX_WIFI_RETRIES = 10;  
const int WIFI_RETRY_DELAY = 1000; 
const char* ssid = "Digicel_WiFi_gVdj";
const char* password = "kPT9sAWB";
AsyncWebServer server(80);

// Forward declaration
void setupServerRoutes();

void setup() {
    // Setup moisture sensor relay
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);

    // OLED initialization
    u8g2.begin();
    u8g2.setFont(u8g2_font_ncenB12_tr);
    
    // Light Sensor initialization
    Wire.begin(D6, D5);
    lightMeter.begin();
    
    // DHT sensor initialization
    dht.begin();
    delay(2000);
    
    // Serial initialization for debugging
    Serial.begin(9600);

    // Attempt WiFi connection
    WiFi.begin(ssid, password);
    int wifiRetries = 0;
    while (WiFi.status() != WL_CONNECTED && wifiRetries < MAX_WIFI_RETRIES) {
        delay(WIFI_RETRY_DELAY);
        Serial.println("Connecting to WiFi...");
        wifiRetries++;
    }
    if(WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Failed to connect to WiFi after multiple attempts");
    }
    setupServerRoutes();  // Set up web server routes
}

void loop() {
    // Read and update moisture
    int rawMoisture = analogRead(MOISTURE_PIN);
    moisturePercentage = map(rawMoisture, 405, 870, 100, 0);

    // Read and update light intensity
    float lightIntensity = lightMeter.readLightLevel();
    lightPercentage = (lightIntensity / 1000.0) * 100.0;
    if (lightPercentage > 100) lightPercentage = 100;

    // Read temperature and humidity
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    // Update plant health status
    updatePlantHealth();

    // Control the relay based on conditions
    if (moisturePercentage < 50) {
        digitalWrite(RELAY_PIN, LOW);  // Turn on water pump
    } else {
        digitalWrite(RELAY_PIN, HIGH); // Turn off water pump
    }

    // Display the values on OLED
    u8g2.firstPage();
    do {
        u8g2.setCursor(0, 15);
        u8g2.print("Moisture: ");
        u8g2.print(moisturePercentage);
        u8g2.print("%");
        u8g2.setCursor(0, 30);
        u8g2.print("Light: ");
        u8g2.print(lightPercentage);
        u8g2.print("%");
        u8g2.setCursor(0, 45);
        u8g2.print("Temp: ");
        u8g2.print(temperature);
        u8g2.setCursor(0, 60);
        u8g2.print("Humidity: ");
        u8g2.print(humidity);
        u8g2.print("%");
    } while (u8g2.nextPage());

    delay(1000);
}

void setupServerRoutes() {
    // Endpoint for current sensor readings
    server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"moisture\":" + String(moisturePercentage) + ",";
        json += "\"light\":" + String(lightPercentage) + ",";
        json += "\"temperature\":" + String(temperature) + ",";
        json += "\"humidity\":" + String(humidity);
        json += "}";
        request->send(200, "application/json", json);
    });

    // Endpoint for plant health status
    server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"status\":\"" + plantHealthStatus + "\",";
        json += "\"reason\":\"" + plantHealthReason + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    server.begin();
}
