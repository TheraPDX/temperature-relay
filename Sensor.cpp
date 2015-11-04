#include "OneWire.h"
#include "Sensor.h"
#include "AverageTemps.h"

bool compareSensorAddresses(const byte lhs[SENSOR_ADDR_SIZE], const byte rhs[SENSOR_ADDR_SIZE]) {
  bool eql = true;
  for(int i = 0; eql != false && i < SENSOR_ADDR_SIZE; i++) {
    if(lhs[i] != rhs[i]) {
      eql = false;
    };
  }
  return eql;
}

float parseTempValue(byte data[12], byte type_s) {
  float celsius;
  
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    if (type_s==1) {    // DS18S20
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10) {
        // "count remain" gives full 12 bit resolution
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
      celsius = (float)raw / 16.0;
      } else { // type_s==2 for DS2438
        if (data[2] > 127) data[2]=0;
        data[1] = data[1] >> 3;
        celsius = (float)data[2] + ((float)data[1] * .03125);
      }
    } else {  // DS18B20 and DS1822
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
    celsius = (float)raw / 16.0;
  }
  return celsius;
}

bool readData(byte data[12], OneWire & ds) {
  byte i;
  bool success = false;
  
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }
  
  if(0xff == data[5] && 0x10 == data[7] && (OneWire::crc8(data, 8) == data[8])) {
    success = true;
  }
  return success;
}


void Sensor::read() {
  byte data[12];
  float celsius, fahrenheit;

  ds.select(addr);
  ds.write(0x44);        // start conversion, with parasite power on at the end
  delay(900);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  ds.reset();
  ds.select(addr);
  ds.write(0xBE, 0);         // Read Scratchpad
  
  if(readData(data, ds)){
    celsius = parseTempValue(data, type);
    fahrenheit = celsius * 1.8 + 32.0;

    if(fahrenheit > (float)-30 && fahrenheit < (float)120) {
      temp = fahrenheit;
      temperatures.erase(temperatures.begin());
      temperatures.push_back(fahrenheit);
      minute_average = averageTemperatures();
    } else {
      Serial.print("Invalid Temperature: ");
      Serial.println(fahrenheit);
      // throw "Invalid Temperature";
    }
  } else {
    Serial.println("Invalid Data CRC");
    // debugPublish("Invalid Data CRC");
    // throw "Invalid Data CRC";
  }
}

float Sensor::averageTemperatures() {
  return averageGreaterThanZero(begin(temperatures), end(temperatures));
}
