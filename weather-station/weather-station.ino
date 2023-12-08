#include <Wire.h>

// ======================================== tmp36 - sensor for temperature
#define TMPPIN A0   

// ======================================== dht11 - sensor for humidity
#include "DHT.h"

#define DHTPIN 2     
#define DHTTYPE DHT11   

DHT dht(DHTPIN, DHTTYPE);

// ======================================== tcs34725 - sensor for sky colour 
#include "Adafruit_TCS34725.h"

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_1X);

// ====================================== readings

enum SkyCondition {
    NIGHT,
    OVERCAST,
    SUNNY,
    UNRECOGNISED
};

struct Readings {
  bool present;
  float humidity;
  float temperature;
  uint16_t colourTemperature;
  uint16_t illuminance;
  SkyCondition skyCondition;
};

struct Readings readings = { false, -1, -1, -1, -1, UNRECOGNISED };

float updateHumidity() {
  float humidity = dht.readHumidity();

  if (isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    readings.present = false; // humidity reading failed so we do not want to send anymore readings to master until its fixed
    return; 
  }

  readings.humidity = humidity;
}

float updateTemperature() {
  int reading = analogRead(TMPPIN);
  
  float voltage = reading * (5.0 / 1024.0);
  
  readings.temperature = (voltage - 0.5) * 100;
}

void updateSkyReadings() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

//  readings.skyCondition = SkyCondition.OVERCAST;
  readings.colourTemperature = tcs.calculateColorTemperature_dn40(r, g, b, c);
  readings.illuminance = tcs.calculateLux(r, g, b);  
}

// ======================================== i2c
const int slaveAddress = 8;

// ======================================== start of program

void setup() {
  Serial.begin(9600);
  
  Serial.println("Starting...");

  // ======================================== I2C
  Wire.begin(slaveAddress); // join the I2C bus at slaveAddress as a slave
  Wire.onRequest(sendAllReadings); // calls sendAllReadings when requested by the master
  
  // ======================================== dht11
  dht.begin();
  
  // ======================================== tcs34725
  if (!tcs.begin()) {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }
  
  // ======================================== 
  Serial.println("Started");
}

union FloatToByteConverter {
  float theFloat;
  byte theBytes[4];
};

void sendFloat(float reading) {
  FloatToByteConverter converter;
  converter.theFloat = reading;
  
  for (int i = 0; i < 4; i++){
    Wire.write(converter.theBytes[i]);
  }
}

void sendInt16(uint16_t reading) {
  Wire.write(reading & 0xff);
  Wire.write(reading >> 8);
}

void sendSkyCondition() {
  uint8_t conditionByte = readings.skyCondition;
  Wire.write(conditionByte);
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

void sendAllReadings() {
  if (!readings.present) {
    Wire.write(0);
    return;
  }
  
  Wire.write(1);
  
  sendFloat(readings.temperature);
  sendFloat(readings.humidity);
  
  sendInt16(readings.colourTemperature);
  sendInt16(readings.illuminance);
  
  sendSkyCondition();
}

void loop() {
  readings.present = true;
  
  updateTemperature();
  updateHumidity();
  updateSkyReadings();

  delay(5000);
}
