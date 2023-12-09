#include <Wire.h>

// ================================= LCD
#include <Adafruit_LiquidCrystal.h>

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
Adafruit_LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

int contrast = 10; // contrast setting for LCD.

//pointers to allow buttons to toggle current station and current menu
int currentStationPtr = 0;
int currentMenuPtr = 0;

// ================================= piezo buzzer
struct Alarm {
  int pin;
  int tone; // tone in HZ to play alarm
  int duration; // duration of each alarm sequence, want it to cycle with breaks of 1000ms until button deactivates the sound.
  bool isOn;
};

struct Alarm alarm = {
  .pin = 10,
  .tone = 400,
  .duration = 1000,
  .isOn = false
};

// ================================= LEDs
// these leds show the conditions
// led at index 0 will be on if the conditions are good, 1 if theyre okay, 2 if theyre poor
const int LEDPins[3] = {A0, A1, A2};

// ================================= buttons
#define NUM_OF_BUTTONS 4

struct Button {
  int pin;
  bool isOn;
};

Button buttons[NUM_OF_BUTTONS] = {
  Button { .pin = 9, .isOn = false },
  Button { .pin = 8, .isOn = false },
  Button { .pin = 7, .isOn = false },
  Button { .pin = 6, .isOn = false }
};

// ================================= times
unsigned long lastRequestTime = 0;
const int requestDelay = 1000; // duration between snapshot requests

unsigned long lastPollTime = 0;
const int pollDelay = 200;

// ================================= stations
struct Prediction {
  String day;
  float temperature; // predicted temperature from the model
  char summary[25]; // holds summary of weather condition prediction passed from model to chatGPT API. max of 25 characters
};

enum SkyCondition {
    NIGHT,
    OVERCAST,
    SUNNY,
    UNRECOGNISED
};

#define NUM_OF_READINGS 5
const char* readingNames[NUM_OF_READINGS] = {"Temperature", "Humidity", "Colour Temperature", "Illuminance", "Sky"};
const char* units[NUM_OF_READINGS-1] = {"C", "%", "K", "Lux"};

struct Readings {
  float humidity;
  float temperature;
  int colourTemperature;
  int illuminance;
  SkyCondition skyCondition;
};

#define NUM_OF_STATIONS 2

struct Station {
    char stationName[20];
    int address; // i2c address
    bool isAvailable;
    Readings readings;
    Prediction predictions[7]; // predictions for each day in the week (next 7 days including today)
};

struct Readings DefaultReadings = {
  .humidity          = -1,
  .temperature       = -1,
  .colourTemperature = -1,
  .illuminance       = -1,
  .skyCondition      = UNRECOGNISED
};

Station stations[NUM_OF_STATIONS] = {
  Station { "Station 1", 8,  false, DefaultReadings, {} },
  Station { "Station 2", 10, false, DefaultReadings, {} }
};

//finds substring of c-style string between indices n and m inclusive which is null terminated.
char* substr(char* s, int n, int m){
  // todo
  Serial.println("Nothing for now");
}

// calculates length of char array which is null terminated.
int charlen(char* str) {
  int len = 0;
  // add one to length each time non null-terminated character reached.
  while(str[len] != '\0'){
      len++;
  }
  return len;
}

// note: using serial print out stops this functions operation
void printCurrentScreen(int stationX, int menuX) {
    lcd.clear();
    
    Station station = stations[currentStationPtr];
    if (currentMenuPtr <= 4) {
        // TODO: DISPLAY AN ERROR IF STATION READINGS ARE NOT AVAILABLE

        //printing sensor data

        //get the reading we want, later will concat unit to this.
        char* readingName = strcat(readingNames[currentMenuPtr], ": "); //get the menu output string
        char reading[6];
        switch (currentMenuPtr) {
          case 0:
            dtostrf(station.readings.temperature, 5, 1, reading);
            break;
          case 1:
            dtostrf(station.readings.humidity, 5, 1, reading);
            break;
          case 2:
            // todo: error if length of integer > 6
            sprintf(reading, "%d", station.readings.colourTemperature);
            break;
          case 3:
            // todo: error if length of integer > 6
            sprintf(reading, "%d", station.readings.illuminance);
            break;
          case 4:
            // todo: show image
            break;
          default:
            // todo
            return;
        };
        
        // print out station name in top row    
        lcd.setCursor(stationX, 0);
        delay(50);
        lcd.print(station.stationName);

        // create the entire display string for bottom row and find its length to allow scrolling off
        char* displayString = strcat(strcat(readingName, reading), units[currentMenuPtr]);
        int displayStringLength = charlen(displayString);

        if (displayStringLength <= 16){
            lcd.setCursor(menuX, 1);
            delay(50);
            lcd.print(displayString);
        } else {
//            ============================================================== TODO:
//            // Scrolling logic for display strings wider than display.
//            // Will work once substring function implemented.
//            int start = 0;
//            int offset = displayStringLength - 16;
//            for (int i = 0; i < offset; i ++) {
//                //take i to be the new start index of the substring
//                lcd.print(substr(displayString, i, displayStringLength - 1));
//                delay(250);
//            }
//            for (int i = 0; i < offset; i --) {
//                lcd.print(substr(displayString, i, displayStringLength - 1));
//                delay(250);
//            }
        }
    } else {
        //printing weekly predictions
        lcd.print("...");
    }
}

void setup() {
    Serial.begin(9600);
    
    Wire.begin(); // initialise I2C communication
    
    pinMode(alarm.pin, OUTPUT); // set the alarm to output
    
    // set buttons 
    for (int i = 0; i < 4; i ++){
        pinMode(buttons[i].pin, INPUT);
    }
    
    // set LED's
    for (int i = 0; i < 3; i ++) {
        pinMode(LEDPins[i], OUTPUT);
    }
    
    analogWrite(13, contrast); // set contrast for UI display.
    
    lcd.begin(16, 2); // set up the LCD's number of columns and rows
    lcd.setBacklight(1);
    printCurrentScreen(3, 0); // print the current screen with the station at the top 5 from the left, and the menu at the bottom 0.
}

void switchStation() {
    // clear the current reading on the screen
    lcd.clear();
    // move to next station (wrap back to initial if we get to the end.)
    //2 since the current number of stations is 2
    currentStationPtr = (currentStationPtr + 1) % NUM_OF_STATIONS;
    // reset to initial menu
    //currentMenuPtr = 0;
    printCurrentScreen(3, 0);
}

// handles switching menus for each station
void switchMenu() {
  lcd.clear();
  
  // scroll off text to give animation for next menu
  for (int positionCounter = 0; positionCounter < 15; positionCounter++){
      //only scroll the bottom section
      printCurrentScreen(3, positionCounter);
      delay(15);
      lcd.clear();
  }

  //update current menu pointer to wrap between values 0, 1, 2 and 3.
  currentMenuPtr = (currentMenuPtr + 1) % NUM_OF_READINGS;
  printCurrentScreen(3, 0);
}

/*
  Reminder - how to use alarm.
  Use tone(alarmPin, alarmTone, alarmDuration) - tone is measured in hertz
  Use noTone(alarmPin) to stop the tone.
*/

bool isEmergency(float readings[]){
    Serial.println("Nothing for now");
}

int getCondition(float readings[]){
    Serial.println("Nothing for now");
}

// ========================= PROTOCOL ===========================
// 1. the first byte determines if there are readings present
//    - 0 = not available, 1 = not available
//    - data type = uint8_t
//    - the communication stops here if no readings are available
// 2. the next 4 bytes is the temperature 
//    - data type = float, units = degrees celsius
// 3. the next 4 bytes is the humidity 
//    - data type = float, units = percentage
// 4. the next 2 bytes is the colour temperature 
//    - data type = uint16_t, units = kelvin
// 5. the next 2 bytes is the illuminance (brightness)
//    - data type = uint16_t, units = lux
// 6. the next byte is the sky condition 
//    - data type = uint8_t
// ==============================================================

#define NUM_OF_REQUIRED_BYTES 14 // 1 + 4 + 4 + 2 + 2 + 1

// allows quick conversion from byte array received via I2C and the float reading out we want.
union FloatToByteConverter {
    float theFloat;
    byte theBytes[4];
};

float convertNextBytesToFloat(int *curByte, byte bytes[NUM_OF_REQUIRED_BYTES]) {
  FloatToByteConverter converter;
  
  for (int i = 0; i < 4; i++) {
    converter.theBytes[i] = bytes[*curByte];
    curByte++;
  };
  
  return converter.theFloat;
}

uint16_t convertNextBytesToInt16(int *curByte, byte bytes[NUM_OF_REQUIRED_BYTES]) {
  byte least_sig = bytes[*curByte]; // todo: add [curByte++] here?
  curByte++;
  
  byte most_sig = bytes[*curByte];
  curByte++;

  return ((uint16_t)most_sig << 8) | least_sig;
}

// getSnapshots procedure supporting current single configured station.
// this procedure is routed by the transmission of array of float readings for the station
void getSnapshots(){
  for (int i = 0; i < NUM_OF_STATIONS; i++) {
    Station* station = &stations[i];

    Wire.beginTransmission(station->address);
    Wire.requestFrom(station->address, NUM_OF_REQUIRED_BYTES);

    int curByte = 0;
    byte bytes[NUM_OF_REQUIRED_BYTES] = {};
    
    for (int i = 0; i < NUM_OF_REQUIRED_BYTES; i++) {
      if (Wire.available()) {
        bytes[i] = Wire.read();
      }
    }

    if (bytes[curByte] == 0) {
      station->isAvailable = false;
      return;
    };
    
    station->isAvailable = true;
    curByte++;

    station->readings.temperature = convertNextBytesToFloat(&curByte, bytes);
    
    station->readings.humidity = convertNextBytesToFloat(&curByte, bytes);

    station->readings.colourTemperature = convertNextBytesToInt16(&curByte, bytes);

    station->readings.illuminance = convertNextBytesToInt16(&curByte, bytes);

    station->readings.skyCondition = bytes[curByte];
    
    Wire.endTransmission();
  }
}

//want to store all of the snapshots by sending them back to the database as a byte array
//database will then sync this with RTC and store it in history (using SD card as storage)
void storeSnapshots () {
    Serial.println("Nothing for now");
}

// DONT NEED THIS ANYMORE AS THIS IS DONE INSIDE getSnapshots?
////then will modify myStations struct, using the new snapshots.
//void updateScreenStateWithSnapshots () {
//    Serial.println("Nothing for now");
//}

//polls buttons on non-interrupt enabled pins.
void pollButtons() {
  for (int i = 0; i < NUM_OF_BUTTONS; i ++) {
      if (digitalRead(buttons[i].pin) == HIGH) {
          buttons[i].isOn = true;
      }
  }
}

void completeActionsFromButtonStates() {
    int i = 0;
    bool doContinue = true; // only allow a single action to be completed per poll cycle.

    while (doContinue && i < NUM_OF_BUTTONS) {
      if (buttons[i].isOn) {
            buttons[i].isOn = false;
            
            switch (i) {
              case 0:
                switchMenu();
                break;
              case 1:
                switchStation();
                break;
              case 2:
                // ========================================= todo
                break;
              case 3:
                noTone(alarm.pin);
                break;
              default:
                // todo: not possible
                return;
            };
            
            doContinue = false;
      };
      i++;
    }
}

void loop() {
    unsigned long curTime = millis();
    
    // make a request to receive data snapshot on regular interval, for now clearing previous entries
    if (curTime - lastRequestTime >= requestDelay) {
        getSnapshots();
        lastRequestTime = curTime;
    }

    //set buttons states based on press events.
    // this is wrapped in debouncer
    if (curTime - lastPollTime >= pollDelay) {
        pollButtons();
        lastPollTime = curTime;
    }

    //check which buttons have been pressed and make corresponding calls to switchMenu(), switchStation(), or noTone(alarmPin).
    completeActionsFromButtonStates();
    
    // TODO: call alarm if isEmergency(readings).
    // if (alarm_condition) and (!alarmFlag) then set alarm flag to true and play alarm.
}
