#include <Adafruit_LiquidCrystal.h>
#include <Wire.h>
#include <math.h>

// initialisations for LCD display and LED outputs 
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
Adafruit_LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

char* currentDisplayString; // the string that is currently being displayed on the lcd

// leds
const int conditionPins[3] = {A0, A1, A2}; 

// alarm
#define ALARM_PIN 10
const unsigned int alarmTone = 400; // frequency in hz
const unsigned long alarmDuration = 1000; // duration in ms
bool alarmFlag = false; // whether the alarm is turned on

// button pins and states
// we store this as an array of structs to converse more memory than using individual arrays 
struct Button {
    int pin;
    bool isOn;
};

Button buttons[4] = {
    Button { .pin = 9, .isOn = false},
    Button { .pin = 8, .isOn = false},
    Button { .pin = 7, .isOn = false},
    Button { .pin = 6, .isOn = false}
};

// storing times for snapshot requests and polling  
unsigned long lastRequestTime = 0;
const int requestDelay = 10000; // in ms

unsigned long lastPollTime = 0;
const int pollDelay = 200; // in ms

unsigned long lastPredictionTime = 0;
const int predictionDelay = 3600000; // in ms //sends new prediction batch every 1hr

unsigned long currentTime;

// holds weather predictions for each weekday from Python time series model
struct Prediction {
    uint8_t id;
    char day[10]; // day predicted.
    float predictedTemperature; // temperature predicted for day.
};

// type casted enum class to uint8 to conserve memory and introduce better type safety 
enum class SkyCondition : uint8_t {
    NIGHT = 0,
    OVERCAST = 1,
    SUNNY = 2,
    UNRECOGNISED = 3
};


#define NUM_OF_READINGS 5

struct Reading {
  char* readingName;
  char* unit;
};

// sky condition isnt a numerical reading so we dont include it here
const Reading readings[NUM_OF_READINGS] = {
    Reading { .readingName = "Humidity: ",              .unit = "%"   },  
    Reading { .readingName = "Temperature: ",           .unit = "C"   },
    Reading { .readingName = "Colour Temperature: ",    .unit = "K"   },
    Reading { .readingName = "Illuminance: ",           .unit = "Lux" },
    Reading { .readingName = "Average predicted temperature for tomorrow: ",   .unit = "C"  }
};

struct Readings {
    float humidity;
    float temperature;
    int colourTemperature;
    int illuminance;
    SkyCondition skyCondition;
};

// todo: revert. modified default readings for UI testing.
struct Readings DefaultReadings = {
    .humidity          = 50,
    .temperature       = 15,
    .colourTemperature = 4000,
    .illuminance       = 700,
    .skyCondition      = SkyCondition :: NIGHT
};

struct Station {
    char stationName[20]; 
    int address; // holds I2C address
    Readings readings;
    Prediction prediction;
    bool isAvailable; // whether the station had readings present for the last snapshot request
};

#define NUM_OF_STATIONS 2
//I2C address should increment up from 8.
Station stations [NUM_OF_STATIONS] = {
    Station { "Station 1", 8, DefaultReadings, {}, false},
    Station { "Station 2", 9, DefaultReadings, {} , false}
};

// pointers to cycle through stations and the menu 
uint8_t currentStationPtr = 0;
uint8_t currentMenuPtr = 0;

// PURPOSE: creating moon, sun, error and cloud icons. The LCD library can store them at locations 0-7. 
void createIcons() {
    byte moon[8] = {
        B00000,
        B00000,
        B00100,
        B00100,
        B01110,
        B10001,
        B01110,
        B00100
    };

    byte sun[8] = {
        B00000,
        B00000,
        B01110,
        B01110,
        B01110,
        B01110,
        B00000,
        B00000,
    };

    byte cloud[8] = {
        B00000,
        B01110,
        B01110,
        B01110,
        B01110,
        B01110,
        B01110,
        B00000,
    };
    // exclamation mark
    byte error[8] = {
        B00000,
        B01110,
        B01110,
        B01110,
        B01110,
        B00000,
        B01110,
        B00000,
    };

    lcd.createChar(0, moon);
    lcd.createChar(1, sun);
    lcd.createChar(2, error);
    lcd.createChar(3, cloud);
}

// PURPOSE: print the icon for the sky condition for a station to the lcd
void printIcon(Station* station) {
    switch (station -> readings.skyCondition) {
        case SkyCondition::NIGHT:
            lcd.write(byte(0)); // moon
            break;
        case SkyCondition::SUNNY:
            lcd.write(byte(1)); // sun
            break;
        case SkyCondition::UNRECOGNISED:
            lcd.write(byte(2)); // exclamation mark 
            break;
        case SkyCondition::OVERCAST:
            lcd.write(byte(3)); // cloud 
            break;
    }
}

// PURPOSE: logic to print out full screen state depending on values of currentStationPtr and currentMenuPtr 
void printCurrentScreen(int stationX = 3, int menuX = 0) {
    lcd.clear();
    bool showingPrediction;
    
    Station* station = &stations[currentStationPtr];
        
    char reading[6];
    // convert each reading to a string
    switch (currentMenuPtr) {
        case 0:
            dtostrf(station -> readings.humidity, 5, 1, reading);
            break;
        case 1:
            dtostrf(station -> readings.temperature, 5, 1, reading);
            break;
        case 2:
            sprintf(reading, "%d" , station -> readings.colourTemperature);
            break;
        case 3:
            sprintf(reading, "%d", station -> readings.illuminance);
            break;
        case 4:
            dtostrf(station -> prediction.predictedTemperature, 5, 1, reading);
            showingPrediction = true;
            break;
        // no case for 4 since icon is displayed alongside station name
    }

    // display station name on lcd
    lcd.setCursor(stationX, 0);
    delay(50);
    lcd.print(station -> stationName);

    // display sky condition icon on lcd
    lcd.setCursor(13, 0);
    delay(50);
    printIcon(station);
    
    char* readingName;

    if (!showingPrediction) {
        readingName = strcat(strdup(readings[currentMenuPtr].readingName), station -> prediction.day);
    } else {
        readingName = strdup(readings[currentMenuPtr].readingName);
    }

    char* displayString  = strcat(strcat(readingName, reading), readings[currentMenuPtr].unit); // create the string for the reading with its name, value and unit

    currentDisplayString = displayString; //set global display string
    
    // display displayString on the bottom row of lcd
    lcd.setCursor(menuX, 1);
    delay(50);
    lcd.print(displayString);
}

#define SCROLL_DELAY 200

// PURPOSE: ability to display strings wider than LCD 
void scrollOff(char* displayString, int displayStringLength, Station* station, int stationX, int menuX) {
    char* substr;

    int offset = (displayStringLength + 5) - 16; // calculate the number of scrolls
       
    for (int i = 0; i < offset; i ++) {
        lcd.clear();
        char* substr = &displayString[i]; // take i to be the new start index of the substring (by making start_pointer = index)
        
        // re print station
        lcd.setCursor(stationX, 0);
        lcd.print(station -> stationName);
        
        lcd.setCursor(13, 0);
        delay(50);
        printIcon(station);
        
        lcd.setCursor(menuX, 1);
        lcd.print(substr);
        
        delay(SCROLL_DELAY);
    }     

    // finally display output as is
    lcd.clear();
    
    lcd.setCursor(stationX, 0);
    lcd.print(station -> stationName);
    
    lcd.setCursor(13, 0);
    delay(50);
    printIcon(station);
    
    lcd.setCursor(menuX, 1);
    lcd.print(displayString);
}

void setup () {
    Serial.begin(9600); // initialise serial monitor
    
    Wire.begin(); // initialise I2C communication

    pinMode(ALARM_PIN, OUTPUT); 

    // set buttons 
    for (int i = 0; i < 4; i ++){
        pinMode(buttons[i].pin, INPUT);
    }

    // set LEDs
    for (int i = 0; i < 3; i ++) {
        pinMode(conditionPins[i], OUTPUT);
    }
    
    analogWrite(13, 10); // set contrast as 10 for UI display, again passing value directly to conserve SRAM space.
   
    lcd.begin(16, 2); // set up the LCD's number of columns and rows
    lcd.setBacklight(1); 

    printCurrentScreen(3, 0); // print the current screen with the station at the top 5 from the left, and the menu at the bottom 0.
}

void switchStation() {
    lcd.clear(); 
    currentStationPtr = (currentStationPtr + 1) % NUM_OF_STATIONS; // wrap to number of stations so we don't scroll off.
    setConditionsForMenu();
    printCurrentScreen();
}

void switchMenu() {
    lcd.clear();
    currentMenuPtr = (currentMenuPtr + 1) % NUM_OF_READINGS; // wrap to number of readings
    setConditionsForMenu();
    printCurrentScreen();
}

// PURPOSE: turns on the alarm if any of the station readings are extreme
void checkForEmergency() {
  int i = 0;

  // we only want the alarm to be turned on once so we use a while loop that ensures alarmFlag is false
  while (!alarmFlag && i < NUM_OF_STATIONS) {
    Station* station = &stations[i];

    // emergency condition for wet-bulb temperature using the Stull formula
    //Note: formula simply models evaporative cooling.
    //Using pow for efficiency compared to sqrt
    //evidence suggest 32 degrees celcius is critical wet bulb temperature.
    //can change this if we have enough free memory to store temp and humidity so we dont have to keep referencing them.
    float T_w = (station -> readings.temperature) * atan(0.151977 * pow((station -> readings.humidity) + 8.313659, 0.5))
    + (0.00391838 * pow ((station -> readings.humidity), 1.5) * atan(0.023101 * (station -> readings.humidity)))
    - atan((station -> readings.humidity) - 1.676331) + atan((station -> readings.temperature) + (station -> readings.humidity))
    - 4.686035;

    // an emergency is when the temperature is less than -5 or greater than 35 degrees celsius
    // can opt to modify the low temperature reading, but evidence is conflicting to the critical low wet-bulb limit.
    if (station -> readings.temperature < -10 || T_w >= 32) {
      alarmFlag = true;
      tone(ALARM_PIN, alarmTone); 
    }

    i++;
  }
}


//simple conditionals to check temperature safety.
uint8_t setConditionForTemperature (float currentReading) {
    if (currentReading < 0) {
        return 2;
    } else if (0 < currentReading < 10) {
        return 1;
    } else if (10 < currentReading < 20) {
        return 0;
    } else if (20 < currentReading < 30) {
        return 1; 
    } else if (currentReading > 30) {
        return 2;
    }
    
}

//based on research for relative humidity scales and adverse health effects.
uint8_t setConditionForHumidity (float currentReading) {
    if (currentReading < 25) {
        return 2;
    } else if (25 < currentReading < 40){
        return 1;
    } else if (40 < currentReading < 60){
        return 0;
    } else if (60 < currentReading < 80){
        return 1;
    } else if (80 < currentReading < 100){
        return 2;
    }
}

uint8_t setConditionForColourTemperature (uint16_t currentReading) {
    if (5500 < currentReading < 6500) {
      return 0;
    } else {
      return 2;
    }
}

uint8_t setConditionForIlluminance (uint16_t currentReading) {
    if (1 < currentReading < 30000) {
      return 0;
    } else {
      return 2;
    }
}

//outputs 0: green / good 1: yellow / moderate 2: red / bad

void setConditionsForMenu() {
    //first get the current menu's reading.
    Readings* currentReadings = &stations[currentStationPtr].readings;
    uint8_t condition;
    //next clear all current LED outputs
    for (uint8_t i = 0; i < 3; i++) {
        digitalWrite(conditionPins[i], LOW);
    }

    Serial.println("Call to check conditions");

    //this method as opposed to accessing by index for memory safety, and also to deal with different dtypes of readings.
    switch (currentMenuPtr) {
        case 0:
            condition = setConditionForHumidity(currentReadings -> humidity);
            break;
        case 1:
            condition = setConditionForTemperature(currentReadings -> temperature);
            break;
        case 2:
            condition = setConditionForColourTemperature(currentReadings -> colourTemperature);
            break;
        case 3:
            condition = setConditionForIlluminance(currentReadings -> illuminance);
            break;
        
    }

    digitalWrite(conditionPins[condition], HIGH);
}



#define NUM_OF_REQUIRED_BYTES 14 // number of bytes to expect from the weather station if readings are present

union FloatToByteConverter {
    float theFloat;
    uint8_t theBytes[4];
};

float convertBytesToFloat(int currentByte, byte bytes[NUM_OF_REQUIRED_BYTES]) {
    FloatToByteConverter converter;

    for (int i = 0; i < 4; i++) {
      converter.theBytes[i] = bytes[currentByte];
      currentByte++;
    }
    
    return converter.theFloat;
}

union Int16ToByteConverter {
    uint16_t theInt;
    byte theBytes[2];
};

uint16_t convertBytesToInt16 (int currentByte, byte bytes[NUM_OF_REQUIRED_BYTES]) {
    Int16ToByteConverter converter;
    
    for (int i = 0; i < 2; i++) {
      converter.theBytes[i] = bytes[currentByte];
      currentByte++;
    }
    
    return converter.theInt;
}

void getSnapshots() {
    for (int i = 0; i < NUM_OF_STATIONS; i ++) {
        Station* station = &stations[i];

        // begin transmission process by requesting required bytes for protocol
        Wire.beginTransmission(station -> address);
        Wire.requestFrom(station -> address, NUM_OF_REQUIRED_BYTES);

        int currentByte = 0; // index of the byte being proccessed
        byte bytes[NUM_OF_REQUIRED_BYTES] = {};

        // get the first byte as we need to determine if more bytes are going to be sent if the readings are present
        if (Wire.available()){
            bytes[0] = Wire.read();
         }
        
        // check if readings are present
        if (bytes[0] == 0) {
            station -> isAvailable = false;
            Wire.endTransmission();
            continue; // go to the next station
        }
        
        station -> isAvailable = true;
        currentByte ++; // we have processed the first byte

        // read all available bytes directly into initialised byte array 
        for (int i = 1; i < NUM_OF_REQUIRED_BYTES; i ++) {
            if (Wire.available()){
                bytes[i] = Wire.read();
            }
        }

        // case for float readings, take next 4 bytes and read them to station struct
        station -> readings.temperature = convertBytesToFloat(currentByte, bytes);
        currentByte += 4;
   
        station -> readings.humidity = convertBytesToFloat(currentByte, bytes);
        currentByte += 4;

        // case for uint16_t readings, takes next two bytes and reads them to station struct 
        station -> readings.colourTemperature = convertBytesToInt16(currentByte, bytes);
        currentByte += 2;

        station -> readings.illuminance = convertBytesToInt16(currentByte, bytes);
        currentByte += 2;

        // case for value of skyCondition enum, casts byte to uint8_t 
        station -> readings.skyCondition =  static_cast<SkyCondition>(bytes[currentByte]);

        Wire.endTransmission();
    }
}

// ! would have used a single generic function for below functions with type checking 
// but Arduino C++ deprecates the use of (type_id) due to runtime constraints. !

byte* convertFloatToBytes(float theFloat) {
    FloatToByteConverter converter;
    converter.theFloat = theFloat;
    return converter.theBytes;
}

byte* convertInt16ToBytes(uint16_t theInt) {
    Int16ToByteConverter converter;
    converter.theInt = theInt;
    return converter.theBytes;
}

// PURPOSE: send data for all stations to the DB 

#define DB_ADDRESS 7

void storeSnapshots() {
    for (uint8_t i = 0; i < NUM_OF_STATIONS; i ++) {
        Station* station = &stations[i];
        Wire.beginTransmission(DB_ADDRESS);
        //write slave address as the unique identifier for the station
        Wire.write(station->address);

        // write float values in sequence of 4 to DB 
        Wire.write(convertFloatToBytes(station -> readings.temperature), 4);
        Wire.write(convertFloatToBytes(station -> readings.humidity), 4);

        // write uint16 values in sequence of 2 to DB 
        Wire.write(convertInt16ToBytes(station -> readings.colourTemperature), 2);
        Wire.write(convertInt16ToBytes(station -> readings.illuminance), 2);

        // send single byte for sky condition last, cast value as write() doesn't accept enumerators 
        Wire.write(static_cast<uint8_t>(station -> readings.skyCondition));
        
        Wire.endTransmission();
    }

}

//PURPOSE - receives all of the predictions for each of the configured weather stations,
//distributes them appropriately based on slave address which is read back in.
void receiveAndDistributePredictions () {
    //begin transmission with DB 
}

// PURPOSE: polls buttons on non-interrupt enabled pins
void pollButtons() {
    for (int i = 0; i < 4; i ++) {
        if (digitalRead(buttons[i].pin) == HIGH) {
            buttons[i].isOn = true;
        }
    }
}

// PURPOSE: calls the appropriate functions based on what button was pressed
//          only one action can be made each time completeActionsFromButtonStates is called
void completeActionsFromButtonStates() {
  int i = 0;
  bool actionMade = false;
  while (!actionMade && i < 4) {
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
              // todo
              Station* station = &stations[currentStationPtr];
              int displayStringLength = strlen(currentDisplayString);
              if (displayStringLength > 16) {
                  scrollOff(currentDisplayString, displayStringLength, station, 3, 0);
              }
              break;
          case 3:
              noTone(ALARM_PIN);
              alarmFlag = false;
              break;
      }
      actionMade = true;
    }  
    i++;
  }
}

bool FIRST_LOOP = true;

void loop () {
    currentTime = millis();
    //initial call to set conditions, only on first loop
    if (FIRST_LOOP) {
        setConditionsForMenu();
        FIRST_LOOP = false;
    }
    // make a request to receive data snapshot on regular interval
    if (currentTime - lastRequestTime >= requestDelay) {
        getSnapshots();
        //emergency condition checked each time new recordings made.
        checkForEmergency();
        storeSnapshots();                 
        lastRequestTime = currentTime;
    }
    
    // make a request to receive predictions on regular interval
    if (currentTime - lastPredictionTime >= predictionDelay) {
        receiveAndDistributePredictions();
        lastPredictionTime = currentTime;
    }

    // set buttons states based on press events, wrapped in debouncer
    if (currentTime - lastPollTime >= pollDelay) {
        pollButtons();
        lastPollTime = currentTime;
    }

    completeActionsFromButtonStates();
}
