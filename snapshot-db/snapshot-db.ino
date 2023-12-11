#include <Wire.h>
#include <DS3231.h>
#include <SD.h>

// !! code assumes values are only read back currently to print predictions, can also modify this to allow history to display on UI !! //

// useful constants //
#define NUM_OF_STATIONS 1
File dataFile; 

// struct to hold predictions fed from Python time series model //
struct Prediction {
    //id to reroute prediction to appropriate station (is their address).
    uint8_t id;
    String day;
    float predictedTemperature;
};

#define NUM_OF_PREDICTIONS 2
Prediction predictions[NUM_OF_STATIONS][NUM_OF_PREDICTIONS];

// structure to store RTC synced time 
struct RTC_Time {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

RTClib myRTC;
RTC_Time currentTime;

// stores all of the readings received by master //

struct Snapshot {
    //unique identifier to replace the station names.
    uint8_t id;
    float temperature;
    float humidity;
    uint16_t colourTemperature;
    uint16_t illuminance;
    uint8_t skyCondition;
    RTC_Time timeRead;
};


// helper functions for type casting //
union BytesToFloat {
    float f;
    byte bytes[4];
};

union BytesToInt16 {
    uint16_t i;
    byte bytes[2];
};

float castBytesToFloat (byte* bytes, int currentByte) {
    union BytesToFloat converter;
    for (int i = 0; i < 4; i++) {
        currentByte ++;
        converter.bytes[i] = bytes[i];
    }
    return converter.f;
};
    
uint16_t castBytesToInt16 (byte* bytes, int currentByte) {
    union BytesToInt16 converter;
    for (int i = 0; i < 2; i++) {
        currentByte ++;
        converter.bytes[i] = bytes[i];
    }
    return converter.i;
};

// setup currently to handle receive requests //
#define DB_ADDRESS 7

void setup () {
    Serial.begin(9600); // initialise serial monitor to read out Python predictions.

    Serial.println("Starting");
    
    // REMINDER: Python script should write predictions to serial over this baud rate
    Wire.begin(DB_ADDRESS); // initialise I2C, join to DB address 7.

    Wire.onReceive(storeSnapshots); // handle receiving readings from master
    Wire.onRequest(sendPredictions); // handle sending predictions back to master

    while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
    }

    Serial.print("Initializing SD card...");
    
    if (!SD.begin(4)) {
      Serial.println("initialization failed!");
      while (1);
    }

    Serial.println("initialization done.");
};


void sendSnapshotTempsToTrainingSet (Snapshot* snapshots) {
    //get temperatures and send to train.csv - use server.py to enable this communication
    for (int i = 0; i < NUM_OF_STATIONS; i++) {
        //send receive command to server.py with the temperature value, time values, and id of station
        //delimited by - to use split('-') in Python script to parse the command values.
        Serial.println("receive - " + String(snapshots[i].id) + " - " 
        + String(snapshots[i].timeRead.year) + " - "  + String(snapshots[i].timeRead.month) + " - " 
        + String(snapshots[i].timeRead.day) + " - " + String(snapshots[i].timeRead.hour) +
        " - " + String(snapshots[i].timeRead.minute) +  " - " + String(snapshots[i].timeRead.second) + 
        " - " + String(snapshots[i].temperature));
    }
};

// store snapshot on SD card
void storeSnapshotsOnSD (Snapshot* snapshots) {    
    //writes to file if already exists and creates new if doesn't (hence handles case of first write)
    dataFile = SD.open("snapshots.txt", FILE_WRITE);
    if (dataFile) {
        for (int i = 0; i < NUM_OF_STATIONS; i++) {
            dataFile.print("ID: ");
            dataFile.println(snapshots[i].id);
            dataFile.print("Temperature: "); //Changed all the below to snapshots[i] now works
            dataFile.println(snapshots[i].temperature);
            dataFile.print("Humidity: ");
            dataFile.println(snapshots[i].humidity);
            dataFile.print("Colour Temperature: ");
            dataFile.println(snapshots[i].colourTemperature);
            dataFile.print("Illuminance: ");
            dataFile.println(snapshots[i].illuminance);
            dataFile.print("Sky Condition: ");
            dataFile.println(snapshots[i].skyCondition);
            dataFile.print("Year: ");
            dataFile.println(snapshots[i].timeRead.year);
            dataFile.print("Month: ");
            dataFile.println(snapshots[i].timeRead.month);
            dataFile.print("Day: ");
            dataFile.println(snapshots[i].timeRead.day);
            dataFile.print("Hour: ");
            dataFile.println(snapshots[i].timeRead.hour);
            dataFile.print("Minute: ");
            dataFile.println(snapshots[i].timeRead.minute);
            dataFile.print("Second: ");
            dataFile.println(snapshots[i].timeRead.second);
        }
    }
    
    dataFile.close();
};


//=========== PROTOCOL FOR REFERENCE ======== //
// For as many stations as are configured
// First 4 bytes is temperature reading (float)
// Second 4 bytes is humidity reading (float)
// Next 2 bytes is colourTemp (uint16_t)
// Next 2 bytes is illuminance (uint16_t)
// Last byte is skyCondition (uint8_t)
// ========================================== //
void storeSnapshots () {
    Snapshot snapshots[NUM_OF_STATIONS];
    for (int i = 0; i < NUM_OF_STATIONS; i ++) {
        int currentByte = 0;
        Snapshot snapshot;

        //store all of the snapshots which are received over I2C.
        while (Wire.available()) {
            snapshot.id = static_cast<uint8_t>(Wire.read());
            currentByte ++;
            snapshot.temperature = castBytesToFloat(Wire.read(), currentByte);
            snapshot.humidity = castBytesToFloat(Wire.read(), currentByte);
            snapshot.colourTemperature = castBytesToInt16(Wire.read(), currentByte);
            snapshot.illuminance = castBytesToInt16(Wire.read(), currentByte);
            snapshot.skyCondition = static_cast<uint8_t>(Wire.read());
            snapshot.timeRead = currentTime;
        };

        snapshots[i] = snapshot;
    }

    // send all of the temperature readings to model training sets
    // need a training set for each of the station names.
    sendSnapshotTempsToTrainingSet(snapshots);
    storeSnapshotsOnSD(snapshots);
};


Prediction* getModelPredictions () {
    // automatic process spawner, run command calls subprocess in Python server script to send batch of predictions.
    Serial.println("run");

    // this should the give us back the predictions as strings containing the station ID to route to, day and temperature reading.

    // keep track of the current day
    uint8_t i = 0;
    // keep track of the current prediction
    uint8_t j = 0;
    while (Serial.available() > 0){
        char day[11];
        uint8_t prediction;
        uint8_t id;

        // Read in integer station ID
        id = Serial.read();

        // Read in packed day string
        for (int k = 0; k < 10; k ++) {
            day[k] = Serial.read();
        }

        // Null-terminate string
        day[10] = '\0'; 

        // Read prediction
        prediction = Serial.read();

        // Store prediction
        predictions[i][j].day = day;
        predictions[i][j].predictedTemperature = prediction;

        //cycle to new station
        if (i % NUM_OF_PREDICTIONS == 0) {
            j ++;
        }
    }
    
};

// sends predictions to the master weather hub
void sendPredictions() {
    getModelPredictions();
    // Sends over I2C
    for (int i = 0; i < NUM_OF_STATIONS; i ++) {
        for (int j = 0; j < NUM_OF_PREDICTIONS; j ++) {
            Prediction* prediction = &predictions[i][j];
            Wire.write(prediction -> id);
            Wire.write(prediction -> day.c_str());

            BytesToFloat converter;
            converter.f = prediction -> predictedTemperature;
            Wire.write(converter.bytes, 4);
        }
    }
};

// main program loop just keeps updating the current time.
void loop () {
    DateTime now = myRTC.now();
    
    currentTime.year = now.year();
    currentTime.month = now.month();
    currentTime.day = now.day();
    currentTime.hour = now.hour();
    currentTime.minute = now.minute();
    currentTime.second = now.second();
};
