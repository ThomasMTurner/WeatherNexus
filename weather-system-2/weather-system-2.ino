//#include <DFRobot_DHT20.h>  (not sure if Dylan used this library)
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
const int pollDelay = 450;

//entry 0 is good, entry 1 is okay, entry 2 is poor. THESE ARE LED'S WHICH SHOW THE CONDITION
const int conditionPins[3] = {A0, A1, A2};
//I2C address 8 for the station, I2C address 9 for the database.
const int slavePins[2] = {8, 9};
const int buttonPins[4] = {9, 8, 7, 6};
// 0 => off, 1 => on
volatile int buttonStates[4] = {0, 0, 0, 0};

unsigned long lastRequestTime = 0;
unsigned long lastPollTime = 0;

bool alarmFlag = false;


//holds predictions for the coming week (7 days including current)
typedef struct {
    //holds weekdays passed from Python script (includes today up to next week).
    String days[7];
    //holds temperature predictions from models for each of the days.
    float predictedTemperature[7];
    //holds summary of weather condition prediction passed from model to chatGPT API.
    String predictedCondition[7];
} Prediction;

//holds each station, its menus (which have been configured based on the sensors provided), and readings for each sensor.
typedef struct {
    String station;
    String menu[4];
    float readings[4];
    int address;
} Station;

typedef struct {
    Prediction p;
    Station s[3];
} ScreenState;

ScreenState screen = {
   {
       {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"},
       {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
       {"Sunny", "Cloudy", "Rainy", "Snowy", "Windy", "Thunderstorm", "Clear"}
   },
   {
    {"Station 1", {"Humidity", "Temperature", "Wind Speed", "Air Pressure"}, {0, 0, 0, 0}, 8},
    {"Station 2", {"Humidity", "Temperature", "Wind Speed", "Air Pressure"}, {0, 0, 0, 0}, 9},
    {"Station 3", {"Humidity", "Temperature", "Wind Speed", "Air Pressure"}, {0, 0, 0, 0}, 10}
   }
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
//0 is predictions screenState and 1 is stations screenState
int currentScreenState = 0;


void setup () {
    // initialise serial monitor
    Serial.begin(9600);
    // initialise I2C communication
    Wire.begin();
    // set the alarm to output
    pinMode(alarmPin, OUTPUT);
    //set buttons 
    for (int i = 0; i < 4; i ++){
        pinMode(buttonPins[i], INPUT);
    }
    //set LED's
    for (int i = 0; i < 3; i ++) {
        pinMode(conditionPins[i], OUTPUT);
    }
    // set contrast for UI display.
    analogWrite(13, contrast);
    // set up the LCD's number of columns and rows
    lcd.begin(16, 2);
    lcd.setBacklight(1);
    //print the current screen with the station at the top 5 from the left, and the menu at the bottom 0.
    printCurrentScreen();
}

void printStationState (int stationX, int menuX) {
    Station currentStation = screen.s[currentStationPtr];
    //station cursor
    lcd.setCursor(stationX, 0);
    lcd.print(currentStation.station);
    //menu cursor
    lcd.setCursor(menuX, 1);
    lcd.print(currentStation.menu[currentMenuPtr] + ": ");
}

void printPredictionState () {
    lcd.setCursor(3, 0);
    lcd.print("Nothing as of yet");
}

//also add state for settings

void printCurrentScreen () {
    if (currentScreenState == 0) {
        printStationState(3, 0);
    }

    else {
        printPredictionState();
    }
}



void switchStation () {
    // clear the current reading on the screen
    lcd.clear();
    // move to next station (wrap back to initial if we get to the end.)
    //2 since the current number of stations is 3
    currentStationPtr = (currentStationPtr + 1) % 2;
    // reset to initial menu
    //currentMenuPtr = 0;
    printStationState(3, 0);
}



//handles switching menus for each station
void switchMenu () {
    lcd.clear();
    // scroll off text to give animation for next menu
    for (int positionCounter = 0; positionCounter < 15; positionCounter ++){
        //only scroll the bottom section
        printStationState(3, positionCounter);
        delay(15);
        lcd.clear();
    }

    currentMenuPtr = (currentMenuPtr + 1) % 3;
    printStationState(3, 0);
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
    //3 being the current number of stations
    for (int i = 0; i < 3; i ++) {
        // get snapshots FOR EVERY station

        //initialise byte array read from I2C
        byte readings[12];
        //initialise convertedReadings to store all three of the final 
        float convertedReadings[3];
        //get the next station to request from
        Station currentStation = screen.s[i];
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
    for (int i = 0; i < 4; i ++) {
        if (digitalRead(buttonPins[i]) == HIGH) {
            buttonStates[i] = 1;
        }
    }
}

void completeActionsFromButtonStates () {
    //configured to only allow a single action to be completed per poll cycle.
    bool actionFlag = false;
    for (int i = 0; i < 4; i ++) {
        if (buttonStates[i] == 1 && !(actionFlag)) {
            //immediate reset of all active button states.
            buttonStates[i] = 0;
            if (i == 0) {
                switchMenu();
                actionFlag = true;
            }

            else if (i == 1) {
                switchStation();
                actionFlag = true;
            }

            else if (i == 2) {
                currentScreenState = (currentScreenState + 1) % 2;
                lcd.clear();
                printCurrentScreen();
                actionFlag = true;
            }

            else if (i == 3) {
                noTone(alarmPin);
                actionFlag = true;
            }
            
        }
    }
}



void loop () {
    unsigned long dt = millis();
    // make a request to receive data snapshot on regular interval, for now clearing previous entries
    if (dt - lastRequestTime >= requestWaitTime) {
        Serial.println("Make another request");
        //I2C procedure to deal with request and changes to myStations.

        //modifies currentReadings in place, consists of currently three float values for each of three stations.
        //getSnapshots(currentReadings);
        lastRequestTime = dt;
    }

    //set buttons states based on press events.
    //this is wrapped in debouncer
    if (dt - lastPollTime >= pollDelay) {
        pollButtons();
        lastPollTime = dt;
    }

    //check which buttons have been pressed and make corresponding calls to switchMenu(), switchStation(), or noTone(alarmPin).
    completeActionsFromButtonStates();


    // call alarm if isEmergency(readings).
    // if (alarm_condition) and (!alarmFlag) then set alarm flag to true and play alarm.
   
}