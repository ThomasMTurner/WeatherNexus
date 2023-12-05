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


// ====================================== extra configurations

//converter from float readings to bytes supporting I2C.
union FloatByteConverter {
  float f;
  byte b[4];
}



// ======================================== start of program

void setup() {
  //sends to sendReadings handler upon request from master.
  Wire.begin(slaveAddress);
  Wire.onRequest(sendReadings);

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


//function to handle sending over readings when requested by master.
void sendReadings (float tmpTemperature) {
    /*
    Solution -
    Send values in specified order, first TMP reading, then DHT, then barometer.
    */

   //will need to modify with outer loop to overwrite converter for each reading.

    //initialise converter
    FloatByteConverter converter;
    //set float value 
    converter.f = tmpTemperature;

    //write byte array to master
    for (int i = 0; i < 4; i ++) {
      Wire.write(converter.b[i]);
    }
    
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
