// ======================================== tmp36
#define TMPPIN A0   

// ======================================== dht11
#include "DHT.h"
//#include <Wire.h>

#define DHTPIN 13    
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

// ======================================== bmp280

//define barometer object
BMP280 bmp;


//....
#define BMP_SCK  (13)
#define BMP_MISO (12)
#define BMP_MOSI (11)
#define BMP_CS   (10)

//I2C slave address for this device.
const int slaveAddress = 8;
//tell me the order of sensors to you will read out so I can configure the master appropriately.
float readings[];
//stores your output of RGB sensor.
int skyCondition;


// ====================================== extra configurations

//converter from float readings to bytes supporting I2C.
union FloatByteConverter {
  float f;
  byte b[4];
}

enum skyConditions: bool {
    NIGHT = false,
    OVERCAST = false,
    SUNNY = false,
    UNRECOGNISED = false
};

int setSkyCondition (int skyCondition, skyConditions conditions) {
    switch (conditions) {
      case NIGHT:
        skyCondition = 0;
      case OVERCAST:
        skyCondition = 1;
      case SUNNY:
        skyCondition = 2;
      case UNRECOGNISED:
        skyCondition = 3;
} 



// ======================================== start of program

void setup() {
  //sends to sendReadings handler upon request from master.
  Wire.begin(slaveAddress);
  //modified I2C transmission, takes readings and RGB sensor value.
  Wire.onRequest(sendReadings(readings, isSunny));

  Serial.begin(9600);

  Serial.println("Starting...");
  
  // ======================================== dht11
  dht.begin();
  
  // ======================================== bmp280
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring or try a different address!");
    while (1) delay(10);
  }

  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
  
  // ======================================== 
  Serial.println("Started");
}

//place to store readings once sensors are working.
//float allReadings [3];



void sendReadings (float readings [], int skyCondition) {
    //here we have set a random length, manually set for this the number of sensors we collect readings for.
    //let me know how many sensors we have, since I'll need to modify the requested number of bytes to (4n + 2) where n is the number of sensors.
    int noOfSensors = 5;
    for (int i = 0; i < noOfSensors; i ++) {
      //create new converter for each read value.
      FloatByteConverter converter;
      //set float value in converter
      converter.f = readings[i];
      //write each float value as byte array to master
      for (int i = 0; i < 4; i ++){
        Wire.write(convert.b[i]);
      }
    }
    
    //split and write the final two bytes as our skyCondition integer
    //high byte transmitted first.
    byte highByte = skyCondition >> 8; 
    byte lowByte = skyCondition & 0xFF;
    Wire.write(highByte);
    Wire.write(lowByte);
  
}

void loop() {
  // ======================================== tmp36 
  int tmpReading = analogRead(TMPPIN);
  
  float tmpVoltage = tmpReading * (5.0 / 1024.0);
  float tmpTemperature = (tmpVoltage - 0.5) * 100;

  Serial.println("--------------");
  
  Serial.print("TMP36 Temperature: ");
  Serial.print(tmpTemperature);
  Serial.println("°C");
    
  // ======================================== dht11  
  float dhtHumidity = dht.readHumidity();
  float dhtTemperature = dht.readTemperature();

  if (isnan(dhtHumidity) || isnan(dhtTemperature)) {
    Serial.println("ERROR: DHT11 READING FAILED");
  } else { 
    Serial.print("DHT11 Humidity: ");
    Serial.print(dhtHumidity);
    Serial.print("%");
    
    Serial.print(" | Temperature: ");
    Serial.print(dhtTemperature);
    Serial.println("°C");
  }
  
  // ======================================== bmp280 
  if (bmp.takeForcedMeasurement()) {
    Serial.print("BMP280 Temperature: ");
    Serial.print(bmp.readTemperature());
    Serial.print("°C");

    Serial.print(" | Pressure: ");
    Serial.print(bmp.readPressure());
    Serial.print("Pa");

    Serial.print(" | Approx Altitude: ");
    Serial.print(bmp.readAltitude(190)); // this parameter has to be adjusted to the local forecast
    Serial.println("m");
  } else {
    Serial.println("ERROR: BMP280 READING FAILED");
  }

  // ======================================== wait 3s
  Serial.println("--------------");
  delay(3000);
}
