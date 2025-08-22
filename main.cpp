#include <Arduino.h>  // This should be first
#include <Wire.h>
#include "bsec2.h"
#include <WiFi.h>
#include <time.h>
#include <PMserial.h>  // For PMS7003
// #include <ArduinoJson.h>
#include "pollution_signatures.h"
#include "pollution_detector.h"

// Define pins for ESP32-S3 DevKitM-1
#define SDA_PIN 8
#define SCL_PIN 9
#define LED_PIN 13
#define PMS_RX_PIN 17  // Connect to PMS7003 TX
#define PMS_TX_PIN 18  // Connect to PMS7003 RX

// WiFi credentials (optional - for NTP time sync)
const char* ssid = "your_ssid";
const char* password = "your_password";

Bsec2 iaqSensor;
SerialPM pms(PMS7003, Serial2);  // PMS7003 sensor

// Function prototypes
void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec);
void checkBsecStatus(Bsec2 bsec);
void errLeds(void);
int8_t i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
int8_t i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
void delay_us(uint32_t period, void *intf_ptr);
bool testI2CConnection(uint8_t address);
void scanI2CDevices();
void updateBaseline(float iaq, float voc, float co2);
bool detectSpike(float iaq, float voc, float co2);
String getTimestamp();
void setupWiFiAndTime();
void outputReading();
void readPMSData();
void printSpikeHistory();

// OPTIMIZED SENSOR CONFIGURATION FOR FASTER READINGS
bsecSensor sensorList[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,  // VOC output in ppm
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_GAS,  // Add raw gas resistance
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY
};

const uint8_t NUM_OUTPUTS = sizeof(sensorList) / sizeof(sensorList[0]);

// OPTIMIZED TIMING CONSTANTS FOR 10-SECOND READINGS
const int BASELINE_SAMPLES = 10; // Reduced for faster baseline establishment
const float SPIKE_THRESHOLD_IAQ = 10.0;  // Reduced from 15.0
const float SPIKE_THRESHOLD_VOC = 0.05;  // Reduced from 0.15
const float SPIKE_THRESHOLD_CO2 = 50.0;  // Reduced from 100.0
const float SPIKE_THRESHOLD_PM25 = 25.0; // PM2.5 spike threshold (µg/m³)
const int MIN_SPIKE_DURATION = 1000; // 1 second
const unsigned long READING_INTERVAL = 10000; // 10 seconds = 10,000 ms
const unsigned long BSEC_CALL_INTERVAL = 1000; // Call BSEC every second

// Baseline and detection variables
float iaqBaseline[BASELINE_SAMPLES];
float vocBaseline[BASELINE_SAMPLES]; // VOC baseline in ppm
float co2Baseline[BASELINE_SAMPLES];
int baselineIndex = 0;
bool baselineReady = false;
bool inSpike = false;
int totalSpikesDetected = 0;
unsigned long spikeStartTime = 0;
unsigned long lastReadingTime = 0;
unsigned long lastBsecCall = 0;
unsigned long startTime = 0;

// Spike event structure
struct SpikeEvent {
    unsigned long startTime;
    unsigned long endTime;
    String signature;
    float maxIaq;
    float maxVoc;
    float maxCo2;
    float maxPm25;
    float maxRawGas;  // Add raw gas tracking
};

PollutionDetector pollutionDetector(SPIKE_THRESHOLD_IAQ, SPIKE_THRESHOLD_VOC, 
    SPIKE_THRESHOLD_CO2, SPIKE_THRESHOLD_PM25);

SpikeEvent currentSpike;
SpikeEvent completedSpikes[10]; // Store last 10 spikes
int spikeHistoryIndex = 0;

// Time tracking
bool timeConfigured = false;
unsigned long bootTime = 0;

// Latest sensor values
float latestTemp = NAN, latestHumidity = NAN, latestPressure = NAN;
float latestIaq = NAN, latestCo2 = NAN, latestVoc = NAN; // VOC in ppm
float latestRawGas = NAN;  // Add raw gas variable
float latestPM1_0 = NAN, latestPM2_5 = NAN, latestPM10_0 = NAN;
bool hasValidData = false;
bool hasPMSData = false;

// I2C communication functions
int8_t i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    uint8_t dev_addr = *(uint8_t*)intf_ptr;
    
    Wire.beginTransmission(dev_addr);
    Wire.write(reg_addr);
    uint8_t rslt = Wire.endTransmission(false);
    if (rslt != 0) {
        return -1;
    }
    
    Wire.requestFrom(dev_addr, len);
    if (Wire.available() != len) {
        return -1;
    }
    
    for (uint32_t i = 0; i < len; i++) {
        reg_data[i] = Wire.read();
    }
    
    return 0;
}

int8_t i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    uint8_t dev_addr = *(uint8_t*)intf_ptr;
    
    Wire.beginTransmission(dev_addr);
    Wire.write(reg_addr);
    for (uint32_t i = 0; i < len; i++) {
        Wire.write(reg_data[i]);
    }
    
    uint8_t rslt = Wire.endTransmission();
    if (rslt != 0) {
        return -1;
    }
    
    return 0;
}

void delay_us(uint32_t period, void *intf_ptr) {
    delayMicroseconds(period);
}

bool testI2CConnection(uint8_t address) {
    Wire.beginTransmission(address);
    Wire.write(0xD0); // Chip ID register
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        return false;
    }
    
    Wire.requestFrom(address, (uint8_t)1);
    if (Wire.available()) {
        uint8_t chipId = Wire.read();
        Serial.printf("Found chip at 0x%02X with ID: 0x%02X ", address, chipId);
        
        if (chipId == 0x61) {
            Serial.println("(BME688)");
            return true;
        } else if (chipId == 0x60) {
            Serial.println("(BME680)");
            return true;
        } else {
            Serial.println("(Unknown chip)");
        }
    }
    
    return false;
}

void scanI2CDevices() {
    Serial.println("Scanning I2C bus...");
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("I2C device found at address 0x%02X\n", address);
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("❌ No I2C devices found!");
    } else {
        Serial.printf("✅ Found %d I2C device(s)\n", deviceCount);
    }
}

void setupWiFiAndTime() {
    // Try to connect to WiFi for NTP time sync
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected!");
        
        // Configure NTP
        configTime(19800, 0, "pool.ntp.org", "time.nist.gov"); // UTC+5:30 for India
        
        // Wait for time to be set
        Serial.print("Waiting for NTP time sync");
        time_t now = time(nullptr);
        int timeAttempts = 0;
        while (now < 1000000000 && timeAttempts < 20) {
            delay(500);
            Serial.print(".");
            now = time(nullptr);
            timeAttempts++;
        }
        
        if (now > 1000000000) {
            Serial.println("\n✅ NTP time synchronized!");
            timeConfigured = true;
        } else {
            Serial.println("\n⚠️ NTP sync failed, using boot time");
        }
    } else {
        Serial.println("\n⚠️ WiFi connection failed, using boot time");
    }
    
    bootTime = millis();
}

String getTimestamp() {
    if (timeConfigured) {
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        return String(timestamp);
    } else {
        // Use boot time + elapsed time
        unsigned long elapsed = millis() - bootTime;
        unsigned long seconds = elapsed / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;
        
        char timestamp[32];
        sprintf(timestamp, "Boot+%02lu:%02lu:%02lu", hours, minutes % 60, seconds % 60);
        return String(timestamp);
    }
}

void readPMSData() {
    switch (pms.read()) {
        case pms.OK:
            latestPM1_0 = pms.pm01;
            latestPM2_5 = pms.pm25;
            latestPM10_0 = pms.pm10;
            hasPMSData = true;
            break;
        case pms.ERROR_TIMEOUT:
            Serial.println("PMS7003: Timeout error");
            break;
        case pms.ERROR_MSG_HEADER:
            Serial.println("PMS7003: Invalid header");
            break;
        case pms.ERROR_MSG_CKSUM:
            Serial.println("PMS7003: Checksum error");
            break;
        default:
            Serial.println("PMS7003: Unknown error");
            break;
    }
}

void printSpikeHistory() {
    Serial.println("\n=== SPIKE HISTORY ===");
    for (int i = 0; i < min(10, totalSpikesDetected); i++) {
        int idx = (spikeHistoryIndex - 1 - i + 10) % 10;
        if (completedSpikes[idx].endTime == 0) continue;
        
        unsigned long duration = completedSpikes[idx].endTime - completedSpikes[idx].startTime;
        Serial.printf("%d. %s - Duration: %.1fs\n", 
                      i+1, 
                      completedSpikes[idx].signature.c_str(), 
                      duration / 1000.0);
        Serial.printf("   Peak - IAQ: %.1f, VOC: %.3fppm, CO2: %.0fppm, RawGas: %.0fΩ",
                     completedSpikes[idx].maxIaq,
                     completedSpikes[idx].maxVoc,
                     completedSpikes[idx].maxCo2,
                     completedSpikes[idx].maxRawGas);
        if (!isnan(completedSpikes[idx].maxPm25)) {
            Serial.printf(", PM2.5: %.1fµg/m³", completedSpikes[idx].maxPm25);
        }
        Serial.println();
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);

    // Initialize pins
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println("=== FAST POLLUTION DETECTOR v8.0 ===");
    Serial.println("10-second readings with timestamps");
    Serial.println("Location: Kolkata, India");
    Serial.println("=====================================");

    // Setup WiFi and time (optional)
    setupWiFiAndTime();

    // Initialize I2C with higher speed for faster communication
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000); // 400kHz for faster I2C
    delay(100);

    // Initialize PMS7003 sensor
    Serial.println("Initializing PMS7003 sensor...");
    Serial2.begin(9600, SERIAL_8N1, PMS_RX_PIN, PMS_TX_PIN);
    pms.init();
    
    Serial.println("Warming up PMS7003...");
    for (int i = 30; i > 0; i--) {
        Serial.printf("%ds remaining...\n", i);
        delay(1000);
    }
    Serial.println("✅ PMS7003 initialized!");

    scanI2CDevices();

    // Find BME688 sensor
    uint8_t bme_addr;
    bool sensorFound = false;
    
    if (testI2CConnection(BME68X_I2C_ADDR_HIGH)) {
        bme_addr = BME68X_I2C_ADDR_HIGH;
        sensorFound = true;
        Serial.println("✅ Using BME688 at address 0x77");
    } else if (testI2CConnection(BME68X_I2C_ADDR_LOW)) {
        bme_addr = BME68X_I2C_ADDR_LOW;
        sensorFound = true;
        Serial.println("✅ Using BME688 at address 0x76");
    }

    if (!sensorFound) {
        Serial.println("❌ BME688 sensor not found!");
        errLeds();
    }

    // Initialize BSEC2
    static uint8_t sensor_addr = bme_addr;
    
    Serial.println("Initializing BSEC2 library...");
    if (!iaqSensor.begin(BME68X_I2C_INTF, i2c_read, i2c_write, delay_us, &sensor_addr)) {
        Serial.println("❌ BSEC2 sensor initialization failed!");
        checkBsecStatus(iaqSensor);
        errLeds();
    }

    Serial.println("✅ BSEC2 sensor initialized!");
    
    // Configure for faster sample rate
    Serial.println("Configuring for optimized 10-second readings...");
    
    // Use LP (Low Power) mode with custom sample rate for balance between speed and accuracy
    float sampleRate = BSEC_SAMPLE_RATE_LP; // 3 seconds internal sampling
    
    if (iaqSensor.updateSubscription(sensorList, NUM_OUTPUTS, sampleRate)) {
        Serial.println("✅ 10-second sensor configuration successful!");
    } else {
        Serial.println("❌ Optimized configuration failed, trying fallback...");
        checkBsecStatus(iaqSensor);
        
        // Fallback to ULP mode
        if (iaqSensor.updateSubscription(sensorList, NUM_OUTPUTS, BSEC_SAMPLE_RATE_ULP)) {
            Serial.println("✅ Fallback ULP configuration successful!");
        } else {
            Serial.println("❌ All configurations failed!");
            errLeds();
        }
    }

    // Attach callback
    iaqSensor.attachCallback(newDataCallback);

    Serial.println("✅ BSEC2 sensor fully configured!");
    Serial.println("🔥 Building baseline... (10 clean air samples needed)");
    
    // Initialize baseline arrays
    for (int i = 0; i < BASELINE_SAMPLES; i++) {
        iaqBaseline[i] = 0;
        vocBaseline[i] = 0;
        co2Baseline[i] = 0;
    }

    // Print available pollution signatures with VOC details
    Serial.println("\n🔍 Available Pollution Signatures (VOC in ppm):");
    for (int i = 0; i < PollutionSignatures::getNumSignatures(); i++) {
        const PollutionPattern* pattern = &PollutionSignatures::getSignatures()[i];
        Serial.printf("  %d. %s - VOC: %.1f-%.1f ppm - %s\n", 
                     i+1, 
                     pattern->name.c_str(), 
                     pattern->minVOC, 
                     pattern->maxVOC,
                     pattern->description.c_str());
    }

    // CSV header - updated with raw_gas_ohms
    Serial.println("\ntimestamp,temp_c,humidity_%,pressure_hpa,iaq,co2_ppm,voc_ppm,raw_gas_ohms,pm1_0,pm2_5,pm10_0,baseline_ready,spike_detected,signature,spike_duration_sec,total_spikes");
    
    startTime = millis();
    lastReadingTime = startTime;
    lastBsecCall = startTime;
}

void loop() {
    unsigned long currentTime = millis();

    static unsigned long lastHistoryPrint = 0;
    if (millis() - lastHistoryPrint > 3600000) { // Every hour
        printSpikeHistory();
        lastHistoryPrint = millis();
    }
    
    // Call BSEC run() every second to ensure it processes data
    if (currentTime - lastBsecCall >= BSEC_CALL_INTERVAL) {
        if (!iaqSensor.run()) {
            checkBsecStatus(iaqSensor);
        }
        lastBsecCall = currentTime;
    }
    
    // Check if it's time for the next reading (10 second interval)
    if (hasValidData && (currentTime - lastReadingTime >= READING_INTERVAL)) {
        outputReading();
        lastReadingTime = currentTime;
    }
    
    // LED status indication
    if (inSpike) {
        // Fast blink during spike
        digitalWrite(LED_PIN, currentTime % 200 < 100);
    } else {
        // Slow blink if baseline not ready, heartbeat if ready
        if (!baselineReady) {
            digitalWrite(LED_PIN, currentTime % 2000 < 100);
        } else {
            digitalWrite(LED_PIN, currentTime % 5000 < 50); // Quick flash every 5 seconds
        }
    }
    
    delay(50); // Reduced delay for more responsive operation
}

void outputReading() {
    if (!hasValidData) return;
    
    // Read PMS data
    readPMSData();
    
    // Update baseline and detect spikes
    updateBaseline(latestIaq, latestVoc, latestCo2);
    bool currentSpikeDetected = detectSpike(latestIaq, latestVoc, latestCo2);
    
    // Detect pollution signature - UPDATED to include rawGas parameter
    auto detection = pollutionDetector.detect(
        latestIaq, latestVoc, latestCo2, latestTemp, latestHumidity, latestRawGas, inSpike
    );
    String signature = detection.signature;
    
    // Handle spike detection logic
    if (currentSpikeDetected && !inSpike) {
        // New spike starting
        inSpike = true;
        spikeStartTime = millis();
        currentSpike.startTime = spikeStartTime;
        currentSpike.signature = signature;
        currentSpike.maxIaq = latestIaq;
        currentSpike.maxVoc = latestVoc;
        currentSpike.maxCo2 = latestCo2;
        currentSpike.maxPm25 = latestPM2_5;
        currentSpike.maxRawGas = latestRawGas;  // Track raw gas max
        currentSpike.endTime = 0; // Reset end time
        
        Serial.printf("🚨 POLLUTION SPIKE DETECTED! Signature: %s\n", signature.c_str());
        Serial.printf("   VOC: %.3f ppm, CO2: %.0f ppm, IAQ: %.1f, RawGas: %.0fΩ\n", 
                     latestVoc, latestCo2, latestIaq, latestRawGas);
        
        if (hasPMSData && !isnan(latestPM2_5)) {
            Serial.printf("   PM2.5: %.1f µg/m³\n", latestPM2_5);
        }
        
    } else if (!currentSpikeDetected && inSpike) {
        // Spike ending
        unsigned long duration = millis() - spikeStartTime;
        if (duration >= MIN_SPIKE_DURATION) {
            totalSpikesDetected++;
            
            // Record completed spike
            currentSpike.endTime = millis();
            completedSpikes[spikeHistoryIndex] = currentSpike;
            spikeHistoryIndex = (spikeHistoryIndex + 1) % 10;
            
            Serial.printf("✅ Spike ended after %lu ms (Total: %d)\n", 
                         duration, totalSpikesDetected);
            Serial.printf("   Duration: %.1f seconds\n", duration / 1000.0);
            Serial.printf("   Peak Values - IAQ: %.1f, VOC: %.3f ppm, CO2: %.0f ppm, RawGas: %.0fΩ",
                         currentSpike.maxIaq, currentSpike.maxVoc, currentSpike.maxCo2, currentSpike.maxRawGas);
            if (!isnan(currentSpike.maxPm25)) {
                Serial.printf(", PM2.5: %.1f µg/m³", currentSpike.maxPm25);
            }
            Serial.println();
        }
        inSpike = false;
    } else if (inSpike) {
        // Update current spike max values
        currentSpike.maxIaq = max(currentSpike.maxIaq, latestIaq);
        currentSpike.maxVoc = max(currentSpike.maxVoc, latestVoc);
        currentSpike.maxCo2 = max(currentSpike.maxCo2, latestCo2);
        currentSpike.maxRawGas = max(currentSpike.maxRawGas, latestRawGas);  // Update raw gas max
        if (!isnan(latestPM2_5)) {
            currentSpike.maxPm25 = max(currentSpike.maxPm25, latestPM2_5);
        }
    }
    
    // CSV output with timestamp - updated to include raw gas
    String timestamp = getTimestamp();
    Serial.printf("%s,%.2f,%.2f,%.2f,%.2f,%.0f,%.3f,%.0f,",
        timestamp.c_str(), latestTemp, latestHumidity, latestPressure, 
        latestIaq, latestCo2, latestVoc, latestRawGas);
    
    // Add PM data
    if (hasPMSData && !isnan(latestPM1_0)) {
        Serial.printf("%.1f,%.1f,%.1f,", latestPM1_0, latestPM2_5, latestPM10_0);
    } else {
        Serial.print(",,,");  // Empty PM values if no data
    }
    
    // Add spike duration if in spike
    if (inSpike) {
        unsigned long duration = millis() - spikeStartTime;
        Serial.printf("%s,%s,%s,%.1f,%d\n",
            baselineReady ? "YES" : "NO",
            "YES",
            signature.c_str(),
            duration / 1000.0,
            totalSpikesDetected);
    } else {
        Serial.printf("%s,%s,%s,,%d\n",
            baselineReady ? "YES" : "NO",
            "NO",
            signature.c_str(),
            totalSpikesDetected);
    }
}

void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec) {
    if (!outputs.nOutputs) {
        return;
    }

    // Extract sensor values and store as latest readings
    for (uint8_t i = 0; i < outputs.nOutputs; i++) {
        const bsecData output = outputs.output[i];
        switch (output.sensor_id) {
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                latestTemp = output.signal;
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                latestPressure = output.signal;
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:
                latestHumidity = output.signal;
                break;
            case BSEC_OUTPUT_RAW_GAS:  // Capture raw gas resistance
                latestRawGas = output.signal;
                break;
            case BSEC_OUTPUT_IAQ:
                latestIaq = output.signal;
                break;
            case BSEC_OUTPUT_CO2_EQUIVALENT:
                latestCo2 = output.signal;
                break;
            case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
                latestVoc = output.signal; // VOC in ppm
                break;
        }
    }

    // Only mark as valid if we have IAQ data
    if (!isnan(latestIaq) && latestIaq > 0) {
        // Set default values for missing data
        if (isnan(latestVoc)) latestVoc = 0.5;
        if (isnan(latestCo2)) latestCo2 = 500;
        if (isnan(latestTemp)) latestTemp = 25;
        if (isnan(latestHumidity)) latestHumidity = 50;
        if (isnan(latestPressure)) latestPressure = 1000;
        if (isnan(latestRawGas)) latestRawGas = 100000;  // Default raw gas value
        
        hasValidData = true;
        
        // For first few readings, output immediately to show progress
        if (!baselineReady && baselineIndex < 3) {
            String timestamp = getTimestamp();
            Serial.printf("Initial reading %d: %s - IAQ: %.1f, VOC: %.3f ppm, CO2: %.0f ppm, RawGas: %.0fΩ\n", 
                         baselineIndex + 1, timestamp.c_str(), latestIaq, latestVoc, latestCo2, latestRawGas);
        }
    }
}

void updateBaseline(float iaq, float voc, float co2) {
    if (!inSpike && iaq > 0 && !isnan(iaq) && !isnan(voc) && !isnan(co2)) {
        iaqBaseline[baselineIndex] = iaq;
        vocBaseline[baselineIndex] = voc; // Store VOC in ppm
        co2Baseline[baselineIndex] = co2;

        baselineIndex = (baselineIndex + 1) % BASELINE_SAMPLES;

        if (!baselineReady && baselineIndex == 0) {
            baselineReady = true;
            Serial.println("✅ Baseline established! Now monitoring for pollution spikes...");
            
            float avgIAQ = 0, avgVOC = 0, avgCO2 = 0;
            for (int i = 0; i < BASELINE_SAMPLES; i++) {
                avgIAQ += iaqBaseline[i];
                avgVOC += vocBaseline[i];
                avgCO2 += co2Baseline[i];
            }
            Serial.printf("📊 Baseline - IAQ: %.1f, VOC: %.3f ppm, CO2: %.0f ppm\n",
                          avgIAQ/BASELINE_SAMPLES, avgVOC/BASELINE_SAMPLES, avgCO2/BASELINE_SAMPLES);
        }
    }
}

bool detectSpike(float iaq, float voc, float co2) {
    if (!baselineReady) return false;
    
    float avgIAQ = 0, avgVOC = 0, avgCO2 = 0;
    for (int i = 0; i < BASELINE_SAMPLES; i++) {
        avgIAQ += iaqBaseline[i];
        avgVOC += vocBaseline[i]; // VOC baseline in ppm
        avgCO2 += co2Baseline[i];
    }
    avgIAQ /= BASELINE_SAMPLES;
    avgVOC /= BASELINE_SAMPLES;
    avgCO2 /= BASELINE_SAMPLES;
    
    bool iaqSpike = (iaq - avgIAQ) > SPIKE_THRESHOLD_IAQ;
    bool vocSpike = (voc - avgVOC) > SPIKE_THRESHOLD_VOC; // VOC spike detection
    bool co2Spike = (co2 - avgCO2) > SPIKE_THRESHOLD_CO2;
    
    // Add PM2.5 spike detection
    bool pm25Spike = false;
    if (hasPMSData && !isnan(latestPM2_5)) {
        pm25Spike = latestPM2_5 > SPIKE_THRESHOLD_PM25;
    }
    
    return (iaqSpike || vocSpike || co2Spike || pm25Spike);
}

void checkBsecStatus(Bsec2 bsec) {
    if (bsec.status < BSEC_OK) {
        Serial.printf("❌ BSEC error code: %d\n", bsec.status);
        if (bsec.status == -1) {
            Serial.println("Configuration failed - incompatible sensor configuration");
        } else if (bsec.status == -2) {
            Serial.println("Device not found - check I2C connection");
        } else if (bsec.status == -3) {
            Serial.println("Invalid input parameters");
        } else if (bsec.status == -32) {
            Serial.println("Config version mismatch");
        } else {
            Serial.println("Unknown BSEC error - check sensor connection and library version");
        }
    } else if (bsec.status > BSEC_OK) {
        Serial.printf("⚠️ BSEC warning code: %d\n", bsec.status);
    }

    if (bsec.sensor.status != 0) {
        Serial.printf("BME68X sensor status: %d\n", bsec.sensor.status);
        if (bsec.sensor.status < 0) {
            Serial.println("❌ BME68X error - check wiring and power supply");
        }
    }
}

void errLeds(void) {
    Serial.println("❌ Critical error - entering error state");
    Serial.println("Reset the board after fixing the issue");
    while (1) {
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
        delay(200);
    }
}