#include "OneWire.h"
#include "Sensors.h"

const char* CHIP_NAME[] = { "DS18S20 or DS1822", "DS18S20", "DS2438", "Unknown" };

const char* getChipName(byte val) {
  int index = (int)val;
  const char* chipName;
  if( index < 0 || index > 2 ) {
    chipName = CHIP_NAME[3];
  }  else {
    chipName = CHIP_NAME[index];
  }
  return chipName;
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

byte chipType(byte a) {
  byte type_s;
  // the first ROM byte indicates which chip
  switch (a) {
    case 0x10:
      //debugPublish("Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      //debugPublish("Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      //debugPublish("Chip = DS1822");
      type_s = 0;
      break;
    case 0x26:
      //debugPublish("Chip = DS2438");
      type_s = 2;
      break;
    // default:
    //   debugPublish("Device is not a DS18x20 family device.");
  }
  return type_s;
}

bool findAndValidateDeviceAddress(uint8_t *addr, OneWire &ds) {
  bool success = true;

  if ( !ds.search(addr)) {
    Serial.println("No More Sensors Found");
    ds.reset_search();
    delay(250);
    success = false;
  } else if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("Sensor CRC is not valid");
    // debugPublish("CRC is not valid!");
    success = false;
  }
  return success;
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
  float accum = 0;
  int count = 0;
  float result = 0;

  for (list<float>::const_iterator it=temperatures.begin(); it != temperatures.end(); ++it) {
    if(*it > 0) {
      accum += *it;
      count++;
    }
  }
  if (count > 0) {
    result = accum/count;
  }
  return result;
}

float Sensors::averageTemperatures() {
  float accum = 0;
  int count = 0;
  float result = 0;

  for (list<Sensor>::const_iterator it=sensors.begin(); it != sensors.end(); ++it) {
    if(it->minute_average > 0) {
      accum += it->minute_average;
      count++;
    }
  }
  if (count > 0) {
    result = accum/count;
  }
  return result;
}

void Sensors::read() {
  for (list<Sensor>::iterator it=sensors.begin(); it != sensors.end(); ++it) {
    if(it->id == 0) continue;

    it->read();
  }

  minute_average = averageTemperatures();
}

void Sensors::scan() {
  byte type_s;
  byte addr[SENSOR_ADDR_SIZE];
  int sensor_id = 1;

  sensors.clear();

  while(findAndValidateDeviceAddress(addr, ds)) {
    Sensor sensor = Sensor(ds);

    type_s = chipType(addr[0]);
    
    sensor.id = sensor_id;
    Serial.print("SensorID: ");
    Serial.print(sensor.id);
    sensor.type = type_s;
    Serial.print("    Sensor Type: ");
    Serial.print(sensor.type);
    Serial.println();
    memcpy(&sensor.addr, &addr, SENSOR_ADDR_SIZE);

    sensors.push_front(sensor);

    sensor_id++;
  }

  ds.reset();
}

void Sensors::debug() {
  Serial.println("## Sensors ##"); 
  for (list<Sensor>::iterator it=sensors.begin(); it != sensors.end(); ++it) {
    if(it->id == 0) continue;

    const char* name = getChipName(it->type); 
    char sensorMessage[75]; 
    snprintf( 
      sensorMessage, 
      75, 
      "Sensor #%i, Type: %i - %s, Address: %x %x %x %x %x %x %x %x", 
      it->id, 
      it->type, 
      name, 
      it->addr[0], 
      it->addr[1], 
      it->addr[2], 
      it->addr[3], 
      it->addr[4], 
      it->addr[5], 
      it->addr[6], 
      it->addr[7] 
    ); 
    Serial.println(sensorMessage); 
  }
}
