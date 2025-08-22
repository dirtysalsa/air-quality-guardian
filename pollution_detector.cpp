#include "pollution_detector.h"

PollutionDetector::PollutionDetector(float iaqThreshold, float vocThreshold, float co2Threshold, float pm25Threshold)
    : _iaqThreshold(iaqThreshold), _vocThreshold(vocThreshold), 
      _co2Threshold(co2Threshold), _pm25Threshold(pm25Threshold) {}

PollutionDetector::DetectionResult PollutionDetector::detect(float iaq, float voc, float co2, float temp, float humidity, float rawGas, bool inSpike) {
    DetectionResult result;
    result.isThreat = false;
    result.isSpike = inSpike;

    // CRITICAL: Add raw gas resistance check for stealth attacks
    // Raw gas values below 10,000Ω indicate severe contamination
    bool lowGasResistance = (rawGas < 10000); // Very low = contamination
    bool suspiciousGasResistance = (rawGas < 25000); // Suspicious level
    
    // ENHANCED THREAT DETECTION - Don't rely only on traditional thresholds
    
    // 1. STEALTH CHEMICAL DETECTION (Low VOC + Low Gas Resistance)
    if (lowGasResistance && voc < 1.0f && iaq >= 40) {
        result.signature = "STEALTH_CHEMICAL_ATTACK_GasRes:" + String(rawGas, 0) + "Ω";
        result.isThreat = true;
        return result;
    }
    
    // 2. AEROSOL DELIVERY DETECTION (High humidity + moderate readings)
    if (humidity > 75 && iaq >= 45 && suspiciousGasResistance) {
        result.signature = "AEROSOL_DELIVERY_Humidity:" + String(humidity, 0) + "%_Gas:" + String(rawGas, 0) + "Ω";
        result.isThreat = true;
        return result;
    }
    
    // 3. ULTRA-LOW VOC ATTACK (Chemical weapons designed to avoid VOC detection)
    if (voc <= 0.5f && iaq >= 45 && suspiciousGasResistance) {
        result.signature = "ULTRA_LOW_VOC_ATTACK_Gas:" + String(rawGas, 0) + "Ω";
        result.isThreat = true;
        return result;
    }
    
    // 4. BORDERLINE READINGS ANALYSIS (Your current situation)
    if (iaq >= 45 && iaq <= 60 && voc <= 1.0f && suspiciousGasResistance) {
        result.signature = "SUSPICIOUS_BORDERLINE_READINGS_Gas:" + String(rawGas, 0) + "Ω";
        result.isThreat = true;
        return result;
    }

    // Original detection logic continues...
    int highestPriority = 99;
    int matchCount = 0;

    for (int i = 0; i < PollutionSignatures::getNumSignatures(); i++) {
        const PollutionPattern& pattern = PollutionSignatures::getSignatures()[i];
        
        bool iaqMatch = (iaq >= pattern.minIAQ) && (iaq <= pattern.maxIAQ);
        bool vocMatch = (voc >= pattern.minVOC) && (voc <= pattern.maxVOC);
        bool co2Match = (co2 >= pattern.minCO2) && (co2 <= pattern.maxCO2);
        bool tempMatch = (temp >= pattern.minTemp) && (temp <= pattern.maxTemp);

        int matches = iaqMatch + vocMatch + co2Match + tempMatch;

        // Enhanced matching for threat signatures
        if (pattern.isThreat) {
            // Lower threshold for threat detection
            if (matches >= 2 && (lowGasResistance || humidity > 70)) {
                matches += 2; // Boost threat signatures
            }
        }

        if (matches >= 3) {
            if (pattern.priority < highestPriority) {
                highestPriority = pattern.priority;
                result.signature = pattern.name;
                result.isThreat = pattern.isThreat;
                matchCount = 1;
            }
            else if (pattern.priority == highestPriority) {
                if (matchCount > 0) {
                    result.signature += "+";
                }
                result.signature += pattern.name;
                matchCount++;
            }
        }
    }

    // FALLBACK ANALYSIS - If no signature matched but readings are suspicious
    if (result.signature.isEmpty()) {
        // Your current readings analysis
        if (iaq <= 60 && voc <= 1.0f && lowGasResistance) {
            result.signature = "STEALTH_CONTAMINATION_GasRes:" + String(rawGas, 0) + "Ω_IAQ:" + String(iaq, 0);
            result.isThreat = true;
        }
        else if (iaq <= 50 && voc <= 0.5f && suspiciousGasResistance) {
            result.signature = "MASKED_ATTACK_GasRes:" + String(rawGas, 0) + "Ω_VOC:" + String(voc, 2) + "ppm";
            result.isThreat = true;
        }
        // Clean air conditions (more restrictive)
        else if (iaq <= 30 && voc <= 0.3f && rawGas > 50000) {
            result.signature = "Clean_Air_IAQ" + String(iaq, 0) + "_VOC" + String(voc, 2) + "ppm_Gas" + String(rawGas, 0) + "Ω";
        }
        // Default analysis for other cases
        else {
            result.signature = "UNKNOWN_ANALYSIS_IAQ" + String(iaq, 0) + "_VOC" + String(voc, 2) + "ppm_Gas" + String(rawGas, 0) + "Ω";
            if (suspiciousGasResistance) {
                result.isThreat = true;
            }
        }
    }

    // Force threat flag for low gas resistance
    if (lowGasResistance) {
        result.isThreat = true;
    }

    return result;
}

bool PollutionDetector::isSpike(float currentValue, float baselineValue, float threshold) const {
    return (currentValue - baselineValue) > threshold;
}

void PollutionDetector::setThresholds(float iaq, float voc, float co2, float pm25) {
    _iaqThreshold = iaq;
    _vocThreshold = voc;
    _co2Threshold = co2;
    _pm25Threshold = pm25;
}