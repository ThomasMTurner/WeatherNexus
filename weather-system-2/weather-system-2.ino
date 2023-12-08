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
const int requestWaitTime = 1000;
const int pollDelay = 200;

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
    //summary should be limited to 25 characters.
    String predictedCondition[7];

} Prediction;

//holds each station, its menus (which have been configured based on the sensors provided), and readings for each sensor.
typedef struct {
    //holds name of station
    char station[20];
    //holds menu names
    char menu[5][20];
    //holds readings for menus
    float readings[4];
    //holds whether the sky is currently overcast (false) or sunny (true)
    bool isSunny;
    //units for each reading
    char units [4][5];
    //holds I2C address
    int address;
    //holds predictions for the coming week.
    Prediction p;
} Station;

//need to change this to reflect new and possibly more sensors used.
Station screen []= {
   {"Station 1", {"Humidity", "Temperature", "Wind Speed", "Air Pressure", "Temp. Predictions"}, {5.0, 7.0, 4.0, 3.0}, 1, {}, 8, {{"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"},
       {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
       {"Sunny", "Cloudy", "Rainy", "Snowy", "Windy", "Thunderstorm", "Clear"}}},
   {"Station 2", {"Humidity", "Temperature", "Wind Speed", "Air Pressure", "Temp. Predictions"}, {4.0, 6.0, 7.0, 3.0}, 1, {}, 10, {{"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"},
       {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
       {"Sunny", "Cloudy", "Rainy", "Snowy", "Windy", "Thunderstorm", "Clear"}}} 
};




// allows quick conversion from byte array received via I2C and the float reading out we want.
union FloatByteConverter {
    float f;
    byte b[4];
};

//stores the readings from getSnapshot for each station.
//currently only stores a single set of 3 values due to single configured station.
float currentReadings[3];
//0 = night, 1 = overcast, 2 = sunny, 3 = unrecognised.
int skyCondition;

//pointers to allow buttons to toggle current station and current menu
int currentStationPtr = 0;
int currentMenuPtr = 0;

//finds substring of c-style string between indices n and m inclusive which is null terminated.
char* substr(char* s, int n, int m){
    Serial.println("Nothing for now");
}

//calculates length of c-style string (char array) which is null terminated.
int charlen(char* str) {
    int len = 0;
    //add one to length each time non null-terminated character reached.
    while(str[len] != '\0'){
        len ++;
    }
    return len;
}


//note: using serial print out stops this functions operation
void printCurrentScreen (int stationX, int menuX) {
    lcd.clear();
    Station currentStation = screen[currentStationPtr];

    if (currentMenuPtr <= 4) {
        //printing sensor data

        //get the reading we want, later will concat unit to this.
        float reading = currentStation.readings[currentMenuPtr];

        //get the menu output string
        char* menuString = strcat(currentStation.menu[currentMenuPtr], ": ");

        //store float output as c-style string in buffer array.
        char buffer[6];
        dtostrf(reading, 5, 1, buffer);
        
        //first print out station name in top row
        //MODIFY THIS TO DISPLAY EITHER CLOUD ICON IF NOT SUNNY, AND SUN ICON IF SUNNY.
        lcd.setCursor(stationX, 0);
        delay(50);
        lcd.print(currentStation.station);

        //next create the entire display string for bottom row and find its length to allow scrolling off
        char* displayString = strcat(currentStation.menu[currentMenuPtr], buffer);
        int displayStringLength = charlen(displayString);

        if (displayStringLength <= 16){
            lcd.setCursor(menuX, 1);
            delay(50);
            lcd.print(displayString);
        }

        else {
            //Scrolling logic for display strings wider than display.
            //Will work once substring function implemented.
            int start = 0;
            int offset = displayStringLength - 16;
            for (int i = 0; i < offset; i ++) {
                //take i to be the new start index of the substring
                lcd.print(substr(displayString, i, displayStringLength - 1));
                delay(250);
            }
            for (int i = 0; i < offset; i --) {
                lcd.print(substr(displayString, i, displayStringLength - 1));
                delay(250);
            }
        }

        

    }

    else {
        //printing weekly predictions
        lcd.print("...");
    }

}





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
    printCurrentScreen(3, 0);
}



void switchStation () {
    // clear the current reading on the screen
    lcd.clear();
    // move to next station (wrap back to initial if we get to the end.)
    //2 since the current number of stations is 2
    currentStationPtr = (currentStationPtr + 1) % 2;
    // reset to initial menu
    //currentMenuPtr = 0;
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

    //update current menu pointer to wrap between values 0, 1, 2 and 3.
    currentMenuPtr = (currentMenuPtr + 1) % 5;
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


//getSnapshots procedure supporting current single configured station.
//this procedure is routed by the transmission of array of float readings for the station
void getSnapshots (float allReadings[3], int skyCondition){
    FloatByteConverter converter;
    //currently considering we have 3 sensors.
    //need (4 * n) + 2 where n is number of sensors, 2 is used for the integer value transmitted.
    int requiredBytes = 14;
    //collect for single station at the moment
    byte readings[requiredBytes];
    bool isSunny;
    float convertedReadings[3];

    Station currentStation = screen[0];
    int currentAddress = currentStation.address;

    //where our integer has been split up.
    byte highByte;
    byte lowByte;

    Wire.beginTransmission(currentAddress);
    Wire.requestFrom(currentAddress, requiredBytes);
    for (int i = 0; i < requiredBytes; i ++) {
        if (Wire.available()) {
            //case of high byte of skyCondition integer 
            if (i == requiredBytes - 2){
                 highByte = Wire.read();
            }
            //case of low byte of skyCondition integer
            if (i == requiredBytes - 1){
                lowByte = Wire.read();
            }
            else {
            //case of reading float.
                readings[i] = Wire.read();
            }
        }
    }

    //modify number of loops based on number of sensors.
    //now we have stored 12 bytes, of which there are three sets of 4 bytes pertaining to each reading
    //1: temp reading 2: .... 3: .....
    for (int i = 0; i < 3; i ++){
        for (int j = 0; j < 4; j ++) {
            converter.b[j] = readings[j];
        }
    allReadings[i] = converter.f;
    }

    //now to print out for testing purposes
    for (int i = 0; i < 3; i ++){
        Serial.println(String(allReadings[i], 2));
    }

    

    Wire.endTransmission();
    //now get the final integer
    skyCondition = (highByte << 8) | lowByte;

}



/*

getSnapshots procedure which can handle multiple stations.

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

*/


//want to store all of the snapshots by sending them back to the database as a byte array
//database will then sync this with RTC and store it in history (using SD card as storage)
void storeSnapshots () {
    Serial.println("Nothing for now");
}

//then will modify myStations struct, using the new snapshots.
void updateScreenStateWithSnapshots () {
    Serial.println("Nothing for now");
}

//polls buttons on non-interrupt enabled pins.
void pollButtons () {
    for (int i = 0; i < 4; i ++) {
        Serial.println(digitalRead(buttonPins[i]));
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
        //Serial.println("Make another request");
        //I2C procedure to deal with request and changes to myStations.
        //for now taking a single reading
        //tempSnapshot();
        //setSkyCondition();
        //modifies currentReadings in place, consists of currently three float values for a single station
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