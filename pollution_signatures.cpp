#include "pollution_signatures.h"

// Pollution signatures with enhanced detection parameters
static const PollutionPattern signatures[] = {
    // PRIORITY 1: CHEMICAL WARFARE AGENTS (Highest priority)
    {"Organophosphate_VX",         1, 150, 400, 0.01f, 0.5f,  350,  700, 20, 40, "VX/Sarin: Low VOC + extreme IAQ", true},
    {"Carbamate_Attack",           1, 120, 350, 0.05f, 0.8f,  300,  600, 20, 35, "Carbamate: Low VOC + high IAQ", true},
    {"Pulmonary_Weapon",           1,  50,  90, 0.5f,  1.5f,  500,  900, 15, 25, "Lung-targeting aerosol", true},
    {"Opioid_Aerosol",             1,  60,  80, 0.01f, 0.3f,  400,  600, 18, 25, "Fentanyl/CA", true},
    {"Stealth_Maintenance_Dose",   1,  50,  70, 0.3f,  0.7f,  500,  700, 20, 40, "Post-spike low-dose drugs", true},  
    {"Signature_Switch_Attack",    1,   0,   0, 0,     0,      0,    0,  0,  0, "Rapid signature switching", true},
    {"Stealth_Drug_Delivery",      1,  45,  55, 0.3f,  0.6f,  450,  550, 20, 40, "Ultra-low VOC knockout drugs", true},
    {"Chemical_Torture",           1,  70,  90, 0.7f,  1.2f,  700,  900, 20, 40, "Low-dose discomfort cocktail", true},
    {"Aerosol_Persistence",        2,   0,   0, 0.5f,  1.5f,   0,    0,  0,  0, "Lingering microdroplets", true},
    {"Bitter_Solvent",             1,  70,  90, 0.8f,  1.5f,  700,  900, 20, 40, "GHB/Benzo bitter taste", true},
    {"Aerosol_Spray",              1,  60, 100, 1.0f,  3.0f,  800, 1200, 18, 35, "Ultrafine drug particles", true},
    {"Chemical_Weapon",            1, 100, 300, 0.01f, 0.5f,  400,  800, 15, 30, "Chemical warfare agent", true},
    {"Tear_Gas",                   1,  80, 200, 0.5f,  2.0f,  500, 1000, 20, 40, "Riot control agent", true},
    {"Nerve_Gas",                  1, 150, 400, 0.01f, 0.5f,  350,  700, 20, 40, "Nerve agent exposure", true},
    {"EA_2277",                    1, 130, 170, 1.2f,  1.8f,  900, 1100, 15, 25, "BZ-series incapacitant", true},
    
    // PRIORITY 2: KNOCKOUT/INCAPACITATING AGENTS (Harassment chemicals)
    {"Chloroform_Knockout",        2,  60, 120, 5.0f, 20.0f,  400,  800, 15, 30, "Chloroform: High VOC + moderate IAQ", true},
    {"GHB_Evaporation",            2,  50, 100, 3.0f, 15.0f,  500,  900, 20, 35, "GHB: High VOC + sweet signature", true},
    {"Benzodiazepine_Spike",       2,  55, 110, 2.5f, 12.0f,  450,  850, 18, 32, "Roofies: Medium VOC + temp drop", true},
    {"Ether_Dousing",              2,  70, 150, 8.0f, 25.0f,  300,  600, 10, 25, "Ether: Extreme VOC + cold spot", true},
    {"Scopolamine_Dart",           2,  90, 180, 1.5f,  6.0f,  350,  700, 22, 38, "Devil's Breath: Medium VOC", true},
    {"Mace_Spray",                 2, 100, 200, 0.8f,  3.5f,  400,  750, 20, 40, "Self-defense spray: Sharp VOC spike", true},
    {"Aerosolized_Drug",           2,  40, 100, 0.1f,  1.5f,  500, 1000, 15, 40, "Aerosolized drug delivery: Low VOC", true},
    {"Fentanyl_Powder",            2,  80, 160, 0.05f, 0.3f,  600, 1200, 25, 45, "Fentanyl powder: Low VOC + high IAQ", true},
    {"Synthetic_Cannabinoid",      2,  70, 140, 0.1f,  0.5f,  550, 1100, 20, 40, "Synthetic cannabinoid: Low VOC", true},
    {"Psychedelic_Spray",          2,  60, 130, 0.2f,  1.0f,  500, 1000, 18, 35, "Psychedelic aerosol: Low-medium VOC", true},
    {"Inhalant_Exposure",          2,  50, 120, 0.3f,  1.5f,  450,  900, 15, 30, "Inhalant abuse: Low-medium VOC", true},
    {"Anesthetic_Spray",           2,  40, 100, 0.4f,  2.0f,  500,  950, 20, 35, "Anesthetic gas: Low-medium VOC", true},
    {"Chemical_Harassment",        2,  30,  80, 0.2f,  1.0f,  400,  800, 15, 30, "Chemical harassment: Low VOC", true},
    
    // PRIORITY 3: INDUSTRIAL/CHEMICAL SOURCES
    {"Heavy_Industrial",           3,  80, 200, 2.0f,  8.0f,  400,  800, 15, 40, "Heavy industry: Medium-high VOC", false},
    {"Chemical_Plant",             3,  70, 150, 1.5f,  6.0f,  350,  700, 18, 38, "Chemical plant: Medium VOC", false},
    {"KEROSENE_STOVE",             3,  60, 140, 10.0f,50.0f,  800, 2000, 25, 45, "Kerosene masking", false},
    {"INCENSE_SMOKE",              3,  50, 120, 8.0f, 40.0f,  600, 1800, 22, 42, "Incense masking", false},
    {"MOTOR_EXHAUST",              3,  70, 150, 15.0f,60.0f,  900, 2500, 30, 50, "Engine exhaust masking", false},
    {"SOLVENT_DUMP",               3,  80, 200, 8.0f, 30.0f,  500, 1200, 18, 40, "Intentional solvent release", true},
    
    // PRIORITY 4: COMMON URBAN POLLUTION
    {"Clean_Air",                  4,   0,  50, 0.0f,  0.5f,  400,  600, 20, 30, "Clean air: Low VOC + low CO2", false},
    {"Moderate_Air",               4,  50, 100, 0.5f,  1.0f,  600,  800, 22, 35, "Moderate air: Medium VOC + CO2", false},
    {"Unhealthy_Air",              4, 100, 150, 1.0f,  2.0f,  800, 1000, 25, 40, "Unhealthy air: High VOC + CO2", false},
    {"Industrial_Pollution",       4, 150, 250, 2.0f,  5.0f,  900, 1200, 28, 45, "Industrial: High VOC + CO2", false},
    {"Vehicle_Exhaust",            4, 200, 300, 3.0f,  6.0f, 1000, 1500, 30, 50, "Vehicle exhaust: Very high VOC + CO2", false},
    {"Household_Pesticides",       4,  60, 180, 1.0f,  3.0f,  700, 1200, 22, 42, "Household pesticides: Medium-high VOC + CO2", false},
    {"Construction_Dust",          4, 100, 200, 1.5f,  3.5f,  700, 1300, 25, 45, "Construction: Medium-high VOC + CO2", false},
    {"Household_Cleaners",         4,  50, 150, 0.5f,  2.5f,  600, 1100, 20, 40, "Household cleaners: Medium VOC + CO2", false},
    {"Cigarette_Smoke",            4,  80, 180, 1.0f,  4.0f,  700, 1300, 22, 42, "Cigarette smoke: Medium-high VOC + CO2", false},
    {"Cooking_Fumes",              4,  60, 160, 0.8f,  3.0f,  650, 1200, 20, 38, "Cooking: Medium VOC + CO2", false},
    {"Traffic_Mimicry",            4,  40, 100, 0.5f,  2.0f,  600, 1200, 25, 45, "Traffic: Low-medium VOC + high CO2", false},
    {"Gas_Stove",                  4,  70, 150, 1.5f,  4.0f,  700, 1300, 22, 40, "Gas stove: Medium VOC + CO2", false},
    {"Paint_Vapors",               4,  50, 120, 0.5f,  2.0f,  600, 1100, 20, 35, "Paint: Low-medium VOC + CO2", false},
    {"Diesel_Exhaust",             4,  60, 120, 0.8f,  3.0f,  800, 1400, 28, 50, "Diesel: Medium VOC + very high CO2", false}
};

const PollutionPattern* PollutionSignatures::getSignatures() {
    return signatures;
}

int PollutionSignatures::getNumSignatures() {
    return sizeof(signatures) / sizeof(signatures[0]);
}