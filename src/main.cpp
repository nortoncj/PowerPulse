#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "EmonLib.h"
#include <driver/adc.h>
#include <WiFi.h>
#include <time.h>

#include "classes/AWSConnector.cpp"

// WIFI
#define WIFI_SSID "Frontier0119"
#define WIFI_PASSWORD "CeleryViolet2526"

// Name Device for AWS IOT
#define DEVICE_NAME "powerPulse"
#define AWS_IOT_ENDPOINT "a25xd37dcl2y9g-ats.iot.us-east-1.amazonaws.com"
#define AWS_IOT_TOPIC "powerPulse/pub"
#define AWS_RECONNECT_ DDLAY 200
#define AWS_MAX_RECONNECT_TRIES 50
#define MEASUREMENT_INTERVAL 1000 // 1 second
#define UPLOAD_COUNT 30 // 30 measurements
#define CALIBRATION_TIME 10000 // 10 S

// NTP Server settings
#define NTP_SERVER "pool.ntp.org"
#define GTM_OFFSET_SEC 0 // Change based on timezone
#define DAYLIGHT_OFFSET_SEC 0 // Change based on daylight savings time

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 4);
EnergyMonitor emon1;
AWSConnector awsConnector;

// Constants
#define ADC_INPUT 34  // GPIO pin for sensor input
#define HOME_VOLTAGE 230.0  // Assuming 230V mains voltage
#define ADC_BITS 10
#define ADC_COUNTS (1<<ADC_BITS)

// Structures to store measurements with timestamps
typedef struct {
  short power;
  double amps;
  unsigned long timestamp;
} PowerMeasurement;

// Variables for timing
PowerMeasurement measurements[UPLOAD_COUNT];
short measureIndex = 0;
unsigned long lastMeasurement = 0;
unsigned long timeFinishedSetup = 0;
bool isCalibrated = true;


void writeEnergyToDisplay(double watts, double amps) {
  lcd.setCursor(0, 0);
  lcd.print("Power: ");
  lcd.print((int)watts);
  lcd.print(" W    ");
  
  lcd.setCursor(0, 3);
  lcd.print("I: ");
  lcd.print(amps * 1000);
  lcd.print(" mA    ");

  
// Print to serial for debugging
Serial.print("Power: ");
Serial.print((int)watts);
Serial.print(" W, ");
Serial.print("Current: ");
Serial.print(amps * 1000, 0);
Serial.println(" mA");
}

void printIPAddress() {
  // lcd.setCursor(0, 2);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP().toString());
}


void connectToWifi() {
  lcd.setCursor(0, 0);
  Serial.println("Connecting to WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("PowerPulse");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  delay(300);

  // Try to connect 10 times to WiFi
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  // If we're still not connected, deep sleep mode and try again
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi");
    esp_sleep_enable_timer_wakeup(1 * 60L * 1000000L);
    esp_deep_sleep_start();
    
  }

  // Connected Confirmation
  Serial.println("Connected to WiFi");
  
  // Print IP Address
  printIPAddress();
  delay(3000);
}

void setupTime() {
  // Connect to NTP server
  configTime(GTM_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  // Wait for time to be set
  time_t now = 0;
  struct tm timeinfo;
  int retry = 0;
  const int maxRetry = 10;

  while(now < 8 * 3600 * 2 && retry < maxRetry) {
  Serial.print("Waiting for time to be set...");
  delay(500);
  time(&now);
  retry++ ;
  }

  if(retry == maxRetry) {
    Serial.println("Failed to connect to NTP server");
  } else {
    localtime_r(&now, &timeinfo);
    Serial.println("Current time: ");
    Serial.println(asctime(&timeinfo));
  }
}

String getFormattedTime() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  char timeStringBuff[25];

  // YYYY-MM-DDTHH:MM:SS
  sprintf(timeStringBuff, "%d-%02d-%02dT%02d:%02d:%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  return String(timeStringBuff);
}

unsigned long getCurrentTimestamp() {
  time_t now;
  time(&now);
  return (unsigned long)now;
}
  
void setup() {

  // Configure ADC
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_12);
  analogReadResolution(ADC_BITS);
  
  // Initialize serial communication
  Serial.begin(115200);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Display startup message
  lcd.setCursor(4, 1);
  lcd.print("Power Pulse");
  delay(5000);
  lcd.clear();

  connectToWifi();
  lcd.clear();

  // Setup time
  lcd.setCursor(0, 0);
  lcd.print("Syncing Time..");
  setupTime();
  delay(3000);
  lcd.clear();
  
  // Initialize emon library
  emon1.current(ADC_INPUT, 30);

  lcd.setCursor(3, 0);
  lcd.print("Pinging Cloud..");
  awsConnector.setup();
  delay(3000);
  lcd.clear();
  

  lcd.setCursor(0, 0);
  lcd.print("Sensor Starting..");
  delay(3000);
  lcd.clear();
  timeFinishedSetup = millis();

  
}


void loop() {
  unsigned long currentMillis = millis();
 

  // Take measurements every second
  if (currentMillis - lastMeasurement > MEASUREMENT_INTERVAL) {
    Serial.println("Taking measurement...");
    
    // Calculate current and power
    double amps = emon1.calcIrms(1480);  // Calculate Irms only
    double watts = amps * HOME_VOLTAGE;

    // Update display
    writeEnergyToDisplay(watts, amps * 1000);

    lastMeasurement = millis();
    
     // If we're past initial stabilization period (5 seconds)
  if (millis() - timeFinishedSetup < CALIBRATION_TIME) {
    lcd.setCursor(0, 0);
    lcd.clear();
    lcd.print("Calibrating... ");
  } else {
    lcd.setCursor(0, 2);
    printIPAddress();
    Serial.println(getCurrentTimestamp());

    // Store measurements with timestamp
    measurements[measureIndex].power = watts;
    measurements[measureIndex].amps = amps;
    measurements[measureIndex].timestamp = getCurrentTimestamp();
    measureIndex++;
  }
    

    if(measureIndex == UPLOAD_COUNT) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Uploading to Cloud.. ");
  
      // Build JSON payload
      String msg = "{";

      // Device ID
      msg += "\"deviceID\": \"" + String(DEVICE_NAME) + "\", ";
      
      // Measurements array
      msg += "\"measurements\": [";
        
      for (short i = 0; i < UPLOAD_COUNT; i++) {
        msg += "{";
        msg += "\"power\": ";
        msg += String(measurements[i].power); // Convert to String
        msg += ", ";
        msg += "\"amps\": ";
        msg += String(measurements[i].amps); // Convert to String
        msg += ", ";
        msg += "\"timestamp\": ";
        msg += String(measurements[i].timestamp); // Convert to String
        msg += "}";
        
        // Add comma if not the last measurement
        if (i < UPLOAD_COUNT - 1) {
          msg += ",";
        }
      }
      
      // Close JSON array and object
      msg += "]";  // Close measurements array
      msg += "}";  // Close main object

      // Send data to AWS
      awsConnector.sendMessage(msg);
      
        measureIndex = 0;
      }
  
      // Keep MQTT connection alive
      awsConnector.loop();
      delay(3000);
      lcd.clear();
  }

  }

  

