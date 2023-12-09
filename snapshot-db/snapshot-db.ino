#include <Wire.h>


// ================== useful constants ================== //
#define NUM_OF_STATIONS 1


// ========= struct to hold predictions fed from Python time series model ========= //

struct Prediction {
    String day;
    float predictedTemperature;
    char summary[25];
}

Prediction predictions[7];

// ======== structure to store RTC synced time ====== //
struct RTC_Time {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}


// ========= stores all of the readings received by master ========= //

struct Snapshot {
    float temperature;
    float humidity;
    uint16_t colourTemperature;
    uint16_t illuminance;
    uint8_t skyCondition;
    RTC_Time timeRead;
}


// ======= helper functions for type casting ====== //
union BytesToFloat {
    float f;
    byte bytes[4];
}

union BytesToInt16 {
    uint16_t i;
    byte bytes[2];
}

float castBytesToFloat (byte* bytes, int currentByte) {
    union BytesToFloat converter;
    for (int i = 0; i < 4; i++) {
        currentByte ++;
        converter.bytes[i] = bytes[i];
    }
    return converter.f;
}
    
uint16_t castBytesToInt16 (byte* bytes, int currentByte) {
    union BytesToInt16 converter;
    for (int i = 0; i < 2; i++) {
        currentByte ++;
        converter.bytes[i] = bytes[i];
    }
    return converter.i;
}

// ========= setup currently to handle receive requests ======== //

void setup () {
    //====== REMINDER: Python script should write predictions to serial over this baud rate ====== //
    Serial.begin(9600); // initialise serial monitor to read out Python predictions.
    Wire.begin(); // initialise I2C.
    Wire.onReceive(storeSnapshots); // handle receiving readings from master.
    Wire.onRequest(sendPredictions); // handle sending predictions back to master.

    // ====== TO DO: below set pins for RTC and SD card modules ====== //
}


//=========== PROTOCOL FOR REFERENCE ======== //

// For as many stations as are configured
// First 4 bytes is temperature reading (float)
// Second 4 bytes is humidity reading (float)
// Next 2 bytes is colourTemp (uint16_t)
// Next 2 bytes is illuminance (uint16_t)
// Last byte is skyCondition (uint8_t)

// ======================= //
void storeSnapshots () {
    for (int i = 0; i < NUM_OF_STATIONS; i++) {
        int currentByte = 0;
        Snapshot snapshot;

        while (Wire.available()) {
            snapshot.temperature = castBytesToFloat(Wire.read(), currentByte);
            snapshot.humidity = castBytesToFloat(Wire.read(), currentByte);
            snapshot.colourTemp = castBytesToInt16(Wire.read(), currentByte);
            snapshot.illuminance = castBytesToInt16(Wire.read(), currentByte);
            snapshot.skyCondition = <static_cast<uint8_t>(Wire.read());
            
        }
    }
}

// ======= TO DO: below store snapshot on SD card. ====== //
void storeSnapshotOnSD (Snapshot snapshot) {
    // ...
}

// ======= TO DO: below get predictions from time series model using serial interface ====== //
Prediction* getModelPredictions () {
    // ...
    if (Serial.available() > 0){
        // ...
    }
    
}

// ====== TO DO: below send predictions from getModelPredictions() to master ====== //
void sendPredictions () {
    //======= gets model predictions from above ======== //
    Prediction* predictions = getModelPredictions();

    //===== sends over I2C, will determine protocol soon ==== //
}