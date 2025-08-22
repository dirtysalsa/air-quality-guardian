#ifndef POLLUTION_DETECTOR_H
#define POLLUTION_DETECTOR_H

#include <Arduino.h>
#include "pollution_signatures.h"

class PollutionDetector {
public:
    // Detection results structure
    struct DetectionResult {
        String signature;
        bool isThreat;
        bool isSpike;
    };

    // Initialize detector with sensitivity thresholds
    PollutionDetector(float iaqThreshold = 10.0f, 
                     float vocThreshold = 0.05f, 
                     float co2Threshold = 50.0f,
                     float pm25Threshold = 25.0f);

    // Detect pollution patterns - UPDATED to include rawGas parameter
    DetectionResult detect(float iaq, float voc, float co2, float temp, float humidity, float rawGas, bool inSpike);

    // Spike detection
    bool isSpike(float currentValue, float baselineValue, float threshold) const;

    // Update detection thresholds
    void setThresholds(float iaq, float voc, float co2, float pm25);

private:
    float _iaqThreshold;
    float _vocThreshold;
    float _co2Threshold;
    float _pm25Threshold;
};

#endif