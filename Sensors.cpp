#include "OneWire.h"
#include "Sensor.h"
#include "Sensors.h"

#include <algorithm>

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

template<typename InputIt, typename MemberType>
float averageGreaterThanZero(InputIt&& begin, InputIt&& end, MemberType Sensor::*var) {
  using value_type = typename iterator_traits<InputIt>::value_type;

  unsigned int count = 0;
  float total = 0;

  for_each(forward<InputIt>(begin), forward<InputIt>(end), [var, &count, &total](const value_type& x) {
    if(x.*var > 0) {
      total += x.*var;
      ++count;
    }
  });

  return count == 0 ? 0 : total / count;
}

float Sensors::averageTemperatures() {
  return averageGreaterThanZero(begin(sensors), end(sensors), &Sensor::temp);
}

float Sensors::minuteAverageTemperatures() {
  return averageGreaterThanZero(begin(sensors), end(sensors), &Sensor::minute_average);
}

void Sensors::read() {
  for (list<Sensor>::iterator it=sensors.begin(); it != sensors.end(); ++it) {
    if(it->id == 0) continue;

    it->read();
  }

  temp = averageTemperatures();
  minute_average = minuteAverageTemperatures();
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
