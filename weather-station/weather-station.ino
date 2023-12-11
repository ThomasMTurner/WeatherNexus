#include <Wire.h>
#include "DHT.h"
#include "Adafruit_TCS34725.h"

#define TMPPIN A1 // temperature sensor

#define DHTPIN A0 // humidity sensor  
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_1X); // colour sensor

enum class SkyCondition : uint8_t {
  NIGHT = 0,
  OVERCAST = 1,
  SUNNY = 2,
  UNRECOGNISED = 3
};

struct Readings {
  bool present; // whether the readings are up to date and correct
  float humidity;
  float temperature;
  uint16_t colourTemperature;
  uint16_t illuminance;
  SkyCondition skyCondition;
};

// current readings. readings are not present.
struct Readings readings = { 
  .present = false, 
  .humidity = -1, 
  .temperature = -1, 
  .colourTemperature = -1, 
  .illuminance = -1, 
  .skyCondition = SkyCondition::UNRECOGNISED 
};

const int slaveAddress = 8; // this weather station's I2C address

// PURPOSE: read the humidity from the DHT11 sensor and store it in the readings struct
// WARNING: its possible for this reading to fail (hence present being set to false)
float updateHumidity() {
  float humidity = dht.readHumidity();

  if (isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    readings.present = false; // humidity reading failed so we do not want to send anymore readings to master until its fixed
    return; 
  }

  readings.humidity = humidity;
}

// PURPOSE: get the temperature from the TMP36 sensor and update its value in the readings struct
float updateTemperature() {
  int reading = analogRead(TMPPIN);
  
  float voltage = reading * (5.0 / 1024.0); // todo: explain these calculations
  
  readings.temperature = (voltage - 0.5) * 100;
}

// PURPOSE: calculate all the readings that are associated with the TCS34725 sensor and update readings struct accordingly
void updateSkyReadings() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  readings.colourTemperature = tcs.calculateColorTemperature_dn40(r, g, b, c);
  readings.illuminance = tcs.calculateLux(r, g, b);  

  if (readings.illuminance < 1) {
    readings.skyCondition = SkyCondition::NIGHT;
    return;
  }
  
  uint32_t sum = c;
  float red, green, blue;
  if (sum == 0) {
    red = green = blue = 0;
  } else {
    red = (float)r / sum;
    green = (float)g / sum;
    blue = (float)b / sum;
  }

  float maxRGB = max(red, max(green, blue));
  float minRGB = min(red, min(green, blue));

  float delta = maxRGB - minRGB;
  float hue;

  if (maxRGB == red) {
    hue = fmod(((green - blue) / delta), 6.0);
  } else if (maxRGB == green) {
    hue = ((blue - red) / delta) + 2;
  } else {
    hue = ((red - green) / delta) + 4;
  }
  
  hue = hue * 60;

  if (hue > 140 && hue < 245) {
    readings.skyCondition = SkyCondition::SUNNY;
  } else if (readings.illuminance < 2000) {
    readings.skyCondition = SkyCondition::OVERCAST;
  } else {
    readings.skyCondition = SkyCondition::UNRECOGNISED;
  }
}

void setup() {
  Serial.begin(9600);
  
  Serial.println("Starting...");

  Wire.begin(slaveAddress); // join the I2C bus at slaveAddress as a slave
  Wire.onRequest(sendAllReadings); // calls sendAllReadings when requested by the master
  
  dht.begin(); 
  
  if (!tcs.begin()) {
    Serial.println("No TCS34725 found ... check your connections");
    while (1); // hang
  }
  
  Serial.println("Started");
}

union FloatToByteConverter {
  float theFloat;
  byte theBytes[4];
};

// PURPOSE:   send a float reading over I2C
// PARAMETER: the reading to be sent
void sendFloat(float reading) {
  FloatToByteConverter converter;
  converter.theFloat = reading;
  
  Wire.write(converter.theBytes, 4);
}

union Int16ToByteConverter {
  uint16_t theInt;
  byte theBytes[2];
};

// PURPOSE:   send a uint16_t reading over I2C
// PARAMETER: the reading to be sent
void sendInt16(uint16_t reading) {
  Int16ToByteConverter converter;
  converter.theInt = reading;
  
  Wire.write(converter.theBytes, 2);
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
  
  Wire.write(1); // readings are present so send 1
  
  sendFloat(readings.temperature);
  sendFloat(readings.humidity);
  
  sendInt16(readings.colourTemperature);
  sendInt16(readings.illuminance);
  
  Wire.write((byte) readings.skyCondition);
}

void loop() {
  readings.present = true;
  
  updateTemperature();
  updateHumidity();
  updateSkyReadings();

  delay(5000);
}
