#include <DFRobot_DHT20.h>
#include <Adafruit_LiquidCrystal.h>
#include <Wire.h>

//initialisations for LCD.
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
Adafruit_LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//contrast setting for LCD.
int contrast = 10;
//pin connected to piezo buzzer.
const int alarmPin = 10;
//tone in HZ to play alarm
const int alarmTone = 400;
//duration of each alarm sequence, want it to cycle with breaks of 1000ms until button deactivates the sound.
const int alarmDuration = 1000;
//duration between snapshot requests, currently set to 5 minutes.
const int requestWaitTime = 30000;

//entry 0 is good, entry 1 is okay, entry 2 is poor. THESE ARE LED'S WHICH SHOW THE CONDITION
const int conditionPins[3] = {A0, A1, A2};
//I2C address 8 for the station, I2C address 9 for the database.
const int slavePins[2] = {8, 9};
const int buttonPins[4] = {6, 7, 8, 9};
// 0 => off, 1 => on
volatile int buttonStates[4] = {0, 0, 0, 0};

unsigned long lastRequestTime = 0;

//holds each station, its menus (which have been configured based on the sensors provided), and readings for each sensor.
typedef struct {
    String station;
    String menu[4];
    float readings[4];
    int address;
} Station;


//initialise stations with empty readings (just for testing).
//will later modify this to build new Station struct and add to stations for each new connected.

//keep track of number of stations, then we will have to initialise new struct with fixed size incremented
//whenever we add a station.
int numberOfStations = 3;
Station myStations[3] = {
    {"Station 1", {"Humidity", "Temperature", "Wind Speed", "Air Pressure"}, {0, 0, 0, 0}, 8},
    {"Station 2", {"Humidity", "Temperature", "Wind Speed", "Air Pressure"}, {0, 0, 0, 0}, 9},
    {"Station 3", {"Humidity", "Temperature", "Wind Speed", "Air Pressure"}, {0, 0, 0, 0}, 10}
};

// allows quick conversion from byte array received via I2C and the float reading out we want.
union FloatByteConverter {
    float f;
    byte b[4];
};

//stores the readings from getSnapshot for each station.
float currentReadings[3][3];

//pointers to allow buttons to toggle current station and current menu
int currentStationPtr = 0;
int currentMenuPtr = 0;


void setup () {
    // initialise serial monitor
    Serial.begin(9600);
    // initialise I2C communication
    Wire.begin();
    // set the alarm to output
    pinMode(alarmPin, OUTPUT);
    //set buttons 
    for (int i = 0; i < 4; i ++){
        pinMode(buttonPins[i], INPUT_PULLUP);
    }
    //set LED's
    for (int i = 0; i < 3; i ++) {
        pinMode(conditionPins[i], OUTPUT);
    }
    // set contrast for UI display.
    analogWrite(13, contrast);
    // set up the LCD's number of columns and rows
    lcd.begin(16, 2);
    //print the current screen with the station at the top 5 from the left, and the menu at the bottom 0.
    printCurrentScreen(3, 0);
}

void printCurrentScreen (int stationX, int menuX) {
    Station currentStation = myStations[currentStationPtr];
    //station cursor
    lcd.setCursor(stationX, 0);
    lcd.print(currentStation.station);
    //menu cursor
    lcd.setCursor(menuX, 1);
    lcd.print(currentStation.menu[currentMenuPtr] + ": ");
}



void switchStation () {
    // clear the current reading on the screen
    lcd.clear();
    // move to next station (wrap back to initial if we get to the end.)
    currentStationPtr = (currentStationPtr + 1) % (numberOfStations - 1);
    // reset to initial menu
    currentMenuPtr = 0;
    printCurrentScreen(3, 0);
}



//handles switching menus for each station
void switchMenu () {
    lcd.clear();
    // scroll off text to give animation for next menu
    for (int positionCounter = 0; positionCounter < 15; positionCounter ++){
        //only scroll the bottom section
        printCurrentScreen(3, positionCounter);
        delay(15);
        lcd.clear();
    }

    currentMenuPtr = (currentMenuPtr + 1) % 3;
    printCurrentScreen(3, 0);
}

/*

Reminder - how to use alarm.
Use tone(alarmPin, alarmTone, alarmDuration) - tone is measured in hertz
Use noTone(alarmPin) to stop the tone.

*/

bool isEmergency (float readings[]){
    Serial.println("Nothing for now");
}

int getCondition (float readings[]){
    Serial.println("Nothing for now");
}

//want a single procedure to request the readings from all stations
//this end receives a byte array which is then converted back to the float list, (as I2C sends one byte at a time, and each float is 4 bytes.)
//return ordered based on I2C address.
//should request 12 bytes, due to 3 float values.



void getSnapshots (float allReadings[3][3]) {
    //initialise byte array to float converter
    FloatByteConverter converter;
    for (int i = 0; i < numberOfStations; i ++) {
        // get snapshots FOR EVERY station

        //initialise byte array read from I2C
        byte readings[12];
        //initialise convertedReadings to store all three of the final 
        float convertedReadings[3];
        //get the next station to request from
        Station currentStation = myStations[i];
        int currentAddress = currentStation.address;
        Wire.beginTransmission(currentAddress);
        Wire.requestFrom(currentAddress, 12);
        //store the three floats (first 4 bytes, second 4 bytes, third 4 bytes) in readings.
        for (int i = 0; i < 12; i ++) {
            if (Wire.available()){
                readings[i] = Wire.read();
            }
        }
        
        //now read the byte array back into float (convertedReadings)
        for (int i = 0; i < 3; i ++) {
            for (int j = 0; j < 4; j ++) {
                converter.b[j] = readings[i * 4 + j];
                allReadings[i][j] = converter.f; 
            }
        } 

        Wire.endTransmission();
    }
}

//want to store all of the snapshots by sending them back to the database as a byte array
//database will then sync this with RTC and store it in history (may want to use external storage unit)
void storeSnapshots () {
    Serial.println("Nothing for now");
}

//then will modify myStations struct, using the new snapshots.
void updateScreenStateWithSnapshots () {
    Serial.println("Nothing for now");
}

void pollButtons () {
    for (int i = 0; i < 4; i ++){
        if (digitalRead(buttonPins[i]) == LOW){
            delay(250);
            buttonStates[i] = 1;
        }
        else {
            delay(250);
            buttonStates[i] = 0;
        }
    }
}


void loop () {
    // make a request to receive data snapshot on regular interval, for now clearing previous entries
    if (millis() - lastRequestTime >= requestWaitTime) {
        Serial.println("Make another request");
        //I2C procedure to deal with request and changes to myStations.
        lastRequestTime = millis();
    }

    pollButtons();

    //check which buttons have been pressed and make corresponding calls to switchMenu(), switchStation(), or noTone(alarmPin)
    for (int i = 0; i < 4; i ++){
        Serial.println(buttonStates[i]);
        if (buttonStates[i] == 1){
            if (i == 0) {
                switchMenu();
            }
            else if (i == 1) {
                switchStation();
            }
            else if (i == 2){
                Serial.println("Nothing for now");
            }
            else if (i == 3) {
                noTone(alarmPin);
            }
        }
    }
    // call alarm if isEmergency(readings)
    
    
}