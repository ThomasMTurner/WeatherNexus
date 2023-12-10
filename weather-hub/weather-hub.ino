#include <Adafruit_LiquidCrystal.h>
#include <Wire.h>

/*

TO DO -

. Add heat index utility function (wet-bulb temperature), and also add this to possible readings values, route output to last reading.
. Create icons for sun, cloud, error, and moon and display on top row.

*/


//=============================== initialisations for LCD display and LED outputs ============================== //


const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
Adafruit_LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


const int conditionPins[3] = {A0, A1, A2}; 


//================ constants for single alarm, store these as constants to conserve SRAM space for LCD outputs. ============ //

#define ALARM_PIN 10
const uint16_t alarmTone = 400;
const uint16_t alarmDuration = 1000;
bool alarmFlag = false;


// ============= button pins and states, store this as struct conserving more memory than arrays ======= //

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


// ==================== storing times for snapshot requests and polling  ======================== //


unsigned long lastRequestTime = 0;
const int requestDelay = 1000;

unsigned long lastPollTime = 0;
const int pollDelay = 200;

unsigned long currentTime;


// ========== Holds weather predictions for each weekday from Python time series model  ======== //


struct Prediction {
    String day; //day predicted.
    float predictedTemperature; //temperature predicted for day.
    char summary [25]; //summary of weather conditions from chatGPT API, max 25 characters.

};


// ========== NOTE:: type casted enum class to uint8 to conserve memory and introduce better type safety  ========= //


enum class SkyCondition : uint8_t {
    NIGHT = 0,
    OVERCAST = 1,
    SUNNY = 2,
    UNRECOGNISED = 3
};


// =================  stores readings for a Station  ======================= //
char* currentDisplayString;

#define NUM_OF_READINGS 5

struct Reading {
  char* readingName;
  char* unit;
};

// sky condition isnt a standard unit so we dont include it here
const Reading readings[NUM_OF_READINGS-1] = {
  Reading { .readingName = "Temperature",        .unit = "%"   },
  Reading { .readingName = "Humidity",           .unit = "K"   },
  Reading { .readingName = "Colour Temperature", .unit = "K"   },
  Reading { .readingName = "Illuminance",        .unit = "Lux" }
};

struct Readings {
    float humidity;
    float temperature;
    int colourTemperature;
    int illuminance;
    SkyCondition skyCondition;
};

//modified default readings for UI testing.
struct Readings DefaultReadings = {
    .humidity          = 5,
    .temperature       = 5,
    .colourTemperature = 5,
    .illuminance       = 5,
    .skyCondition      = SkyCondition :: NIGHT
};


// =========== holds each station, its menus (which have been configured based on the sensors provided), and readings for each sensor. ========= //


struct Station {
    char stationName[20]; 
    int address; //holds I2C address
    Readings readings;
    Prediction predictions[7];
    bool isAvailable;
};

#define NUM_OF_STATIONS 2
Station stations [NUM_OF_STATIONS] = {
    Station { "Station 1", 8, DefaultReadings, {}, false},
    Station { "Station 2", 10, DefaultReadings, {} , false}
};


// ====== pointers to allow buttons to toggle current station and current menu ======= //


uint8_t currentStationPtr = 0;
uint8_t currentMenuPtr = 0;


// ===== creating moon, sun, error and cloud icons, LCD memory can store them at locations 0-7. Also printing logic for icons. ===== //


void createIcons () {
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

    byte sun[] = {
        B00000,
        B00000,
        B01110,
        B01110,
        B01110,
        B01110,
        B00000,
        B00000,
    };

    byte cloud[] = {
        B00000,
        B01110,
        B01110,
        B01110,
        B01110,
        B01110,
        B01110,
        B00000,
    };
    //simple exclamation mark.
    byte error[] = {
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


void printIcon (Station* station) {
    //----------- Access to SkyCondition enumerators like this due to use of enum class --------- //
    switch (station -> readings.skyCondition) {

        case SkyCondition::NIGHT:
            //moon for night.
            lcd.write(byte(0));
            break;

        case SkyCondition::SUNNY:
            //sun for sunny.
            lcd.write(byte(1));
            break;

        case SkyCondition::UNRECOGNISED:
            //error exclamation for UNRECOGNISED.
            lcd.write(byte(2));
            break;

        case SkyCondition::OVERCAST:
            //cloud for overcast.
            lcd.write(byte(3));
            break;
    
    }
}

// ======= logic to print out full screen state depending on values of currentStationPtr and currentMenuPtr ===== //


void printCurrentScreen (int stationX = 3, int menuX = 0) {
    lcd.clear();
    Station* station = &stations[currentStationPtr];
    //modified to make copy of name due to type conflicts.
    char* readingName = strdup(readings[currentMenuPtr].readingName);
    //case of printing sensor data
    if (currentMenuPtr <= 4) {
        // ======= TO DO: display error if readings not available ========= //
        char reading[6];
        switch (currentMenuPtr) {
            case 0:
                dtostrf(station -> readings.temperature, 5, 1, reading);
                break;
            case 1:
                dtostrf(station -> readings.humidity, 5, 1, reading);
                break;
            case 2:
                sprintf(reading, "%d" , station -> readings.colourTemperature);
                break;
            case 3:
                sprintf(reading, "%d", station -> readings.illuminance);
                break;
            //no case for 4 since icon is displayed alongside station name
        }

        // ========= Displaying station name alongside appropriate icon for sky reading ======== //

        lcd.setCursor(stationX, 0);
        delay(50);
        lcd.print(station -> stationName);

        lcd.setCursor(13, 0);
        delay(50);
        printIcon(station);
        

        //====== create entire display string, store its length, reposition cursor and store display string globally for scrollOff() functionality =======//
        char* displayString = strcat(strcat(readingName, reading), readings[currentMenuPtr].unit);
        currentDisplayString = displayString;
        int displayStringLength = strlen(displayString);
        lcd.setCursor(menuX, 1);
        delay(50);
        lcd.print(displayString);
        
    }

    else {
        //printing weekly predictions
        lcd.print("...");
    }

}


// ====== wraps logic for display strings wider than LCD ====== //
#define SCROLL_DELAY 200

void scrollOff (char* displayString, int displayStringLength, Station* station, int stationX, int menuX) {
    char* substr;

    //===== determine number of scrolls ======//
    int offset = (displayStringLength + 5) - 16;

       
    for (int i = 0; i < offset; i ++) {
        lcd.clear();
        //==== take i to be the new start index of the substring (by making start_pointer = index) ===//
        char* substr = &displayString[i];
        //===== need to also re print station ===//
        lcd.setCursor(stationX, 0);
        lcd.print(station -> stationName);
        lcd.setCursor(13, 0);
        delay(50);
        printIcon(station);
        lcd.setCursor(menuX, 1);
        lcd.print(substr);
        delay(SCROLL_DELAY);
    }
          

     //==== then finally display output as is =====//
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

    //set buttons 
    for (int i = 0; i < 4; i ++){
        pinMode(buttons[i].pin, INPUT);
    }

    //set LED's
    for (int i = 0; i < 3; i ++) {
        pinMode(conditionPins[i], OUTPUT);
    }
    
    analogWrite(13, 10); // set contrast as 10 for UI display, again passing value directly to conserve SRAM space.
   
    lcd.begin(16, 2); // set up the LCD's number of columns and rows
    lcd.setBacklight(1); 

    printCurrentScreen(3, 0); //print the current screen with the station at the top 5 from the left, and the menu at the bottom 0.
}


void switchStation () {
    lcd.clear(); 
    currentStationPtr = (currentStationPtr + 1) % NUM_OF_STATIONS; //wrap to number of stations so we don't scroll off.
    printCurrentScreen();
}

void switchMenu () {
    lcd.clear();
    currentMenuPtr = (currentMenuPtr + 1) % (NUM_OF_READINGS - 1); //wrap to number of readings (-1 as we omitted sky condition).
    printCurrentScreen();
}



bool isEmergency (float readings[]){
    Serial.println("Nothing for now");
}

int getCondition (float readings[]){
    Serial.println("Nothing for now");
}


//========== utility functions to convert between dtypes during transmission ========== //


#define NUM_OF_REQUIRED_BYTES 14

union FloatToByteConverter {
    float theFloat;
    byte theBytes[4];
};

float convertBytesToFloat(int *currentByte, byte bytes[NUM_OF_REQUIRED_BYTES]) {
    FloatToByteConverter converter;

    for (int i = 0; i < 4; i ++) {
        converter.theBytes[i] = bytes[*currentByte];
        currentByte ++;
    }
    
    return converter.theFloat;
}

//!! below union may be helpful in Dylan's above conversion function !!//

union Int16ToByteConverter {
    uint16_t theInt;
    byte theBytes[2];
};

uint16_t convertBytesToInt16 (int *currentByte, byte bytes[NUM_OF_REQUIRED_BYTES]) {
    Int16ToByteConverter converter;

    for (int i = 0; i < 2; i ++) {
        converter.theBytes[i] = bytes[*currentByte];
        currentByte ++;
    }
    
    return converter.theInt;
}


// ========== PROTOCOL ================ //

// 1. First byte determines if there are readings present
//  - 0 = not available, 1 = available
//  - dtype = bool (I think bool is preferable over uint8_t)
//  - communication stops here if no readings available
// 2. Next 4 bytes is temperature
//  - dtype = float, units = celcius
// 3. Next 4 bytes is humidity
//  -dtype = float, units = percentage
// 4. Next 2 bytes is colour temperature 
//  -dtype = uint16_t, units = kelvin
// 5. Next 2 bytes is illuminance (brightness)
//  -dtype = uint16_t, units = lux
// 6. Last byte is sky condition
//  -dtype = uint8_t
// May want to add for docs - efficient for communication since never exceeds 32 byte buffer for I2C.
// ===================================== //


void getSnapshots () {
    for (int i = 0; i < NUM_OF_STATIONS; i ++) {
        Station* station = &stations[i];

        //===== begin transmission process by requesting required bytes for protocol ==== //
        Wire.beginTransmission(station -> address);
        Wire.requestFrom(station -> address, NUM_OF_REQUIRED_BYTES);

        int currentByte = 0;
        byte bytes[NUM_OF_REQUIRED_BYTES] = {};

        if (Wire.available()){
            bytes[0] = Wire.read();
         }
        
        // ===== check if readings are present ==== //
        if (bytes[0] == 0) {
            station -> isAvailable = false;
            continue;
        }
        
        station -> isAvailable = true;
        currentByte ++;

        //===== read all available bytes directly into initialised byte array ==== //
        for (int i = 1; i < NUM_OF_REQUIRED_BYTES; i ++) {
            if (Wire.available()){
                bytes[i] = Wire.read();
            }
        }


        //========== case for float readings, take next 4 bytes and read them to station struct  ========= //
        station -> readings.temperature = convertBytesToFloat(&currentByte, bytes);
        station -> readings.humidity = convertBytesToFloat(&currentByte, bytes);

        //======= case for uint16_t readings, takes next two bytes and reads them to station struct ====== //
        station -> readings.colourTemperature = convertBytesToInt16(&currentByte, bytes);
        station -> readings.illuminance = convertBytesToInt16(&currentByte, bytes);

        //====== case for value of skyCondition enum, casts byte to uint8_t ====== //
        station -> readings.skyCondition =  static_cast<SkyCondition>(bytes[currentByte]);

        Wire.endTransmission();
    }
}

//=========== utility functions for storeSnapshots() ======= //

// ! would have used a single generic function for below with type checking but Arduino C++ deprecates the use of (type_id) due to runtime constraints. !//

byte* convertFloatToBytes (float theFloat) {
    FloatToByteConverter converter;
    converter.theFloat = theFloat;
    return converter.theBytes;
}

byte* convertInt16ToBytes (uint16_t theInt) {
    Int16ToByteConverter converter;
    converter.theInt = theInt;
    return converter.theBytes;
}

// ======= Simple protocol to send stations data to the DB ======= //

void storeSnapshots() {
    for (int i = 0; i < NUM_OF_STATIONS; i ++) {
        Station* station = &stations[i];
        Wire.beginTransmission(9);

        // ====== write float values in sequence of 4 to DB ======== //
        Wire.write(convertFloatToBytes(station -> readings.temperature), 4);
        Wire.write(convertFloatToBytes(station -> readings.humidity), 4);

        // ===== write uint16 values in sequence of 2 to DB ============ //
        Wire.write(convertInt16ToBytes(station -> readings.colourTemperature), 2);
        Wire.write(convertInt16ToBytes(station -> readings.illuminance), 2);

        // ===== send single byte for sky condition last, cast value as write() doesn't accept enumerators ======== //
        Wire.write(static_cast<uint8_t>(station -> readings.skyCondition));
        Wire.endTransmission();
    }

}


// =========== polls buttons on non-interrupt enabled pins. ========= //
void pollButtons () {
    for (int i = 0; i < 4; i ++) {
        if (digitalRead(buttons[i].pin) == HIGH) {
            buttons[i].isOn = true;
        }
    }
}

// ========= self explanatory, modified to use switch and break statements ======== //
void completeActionsFromButtonStates () {
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
              // ===== TO DO! ===== //
              Station* station = &stations[currentStationPtr];
              int displayStringLength = strlen(currentDisplayString);
              if (displayStringLength > 16) {
                  scrollOff(currentDisplayString, displayStringLength, station, 3, 0);
              }
              break;
          case 3:
              noTone(ALARM_PIN);
              break;
      }
      actionMade = true;
    }  
    i++;
  }
}



void loop () {
    currentTime = millis();

    //========== make a request to receive data snapshot on regular interval, for now clearing previous entries ========//
    if (currentTime - lastRequestTime >= requestDelay) {
        getSnapshots();                    
        lastRequestTime = currentTime;
    }

    //========= set buttons states based on press events, wrapped in debouncer ========//
    if (currentTime - lastPollTime >= pollDelay) {
        pollButtons();
        lastPollTime = currentTime;
    }

    completeActionsFromButtonStates();
    //===== TO DO!: if (emergency) and (!alarmFlag) then set alarm flag to true and play alarm. =====//
}
