#include "OneWire.h"
#include "Sensor.h"
#include "Sensors.h"
#include "AverageTemps.h"

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
  switch (a) {
  case 0x10:
    type_s = 1;
    break;
  case 0x28:
    type_s = 0;
    break;
  case 0x22:
    type_s = 0;
    break;
  case 0x26:
    type_s = 2;
    break;
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
    success = false;
  }
  return success;
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

bool isByteArrayEmpty(byte b[]) {
  bool empty = true;
  for(int i = 0; empty == true && i < sizeof(b); i++) {
    if(b[i] != 0x00) empty = false;
  }
  return empty;
}

void Sensors::scan() {
  byte type_s;
  byte addr[SENSOR_ADDR_SIZE];
  byte sensorAddrs[MAX_SENSORS][SENSOR_ADDR_SIZE];
  int sensor_id = 1;
  bool found = false;

  // Zero out all the sensor addresses;
  for(int i = 0; i < MAX_SENSORS; i++) {
    for(int j = 0; j < SENSOR_ADDR_SIZE; j++) {
      sensorAddrs[i][j] = 0x00;
    }
  }

  // Read up to 10 sensors off the bus
  while(sensor_id <= MAX_SENSORS && findAndValidateDeviceAddress(addr, ds)) {
    memcpy(sensorAddrs[sensor_id], &addr, SENSOR_ADDR_SIZE);
  }

  // For each new sensor read, add it to the sensors list if it doesn't exist
  for(int i = 0; i < MAX_SENSORS; i++) {
    if(isByteArrayEmpty(sensorAddrs[i])) continue;

    found = false;
    for(list<Sensor>::const_iterator iter = sensors.begin(); iter != sensors.end(); ++iter) {
      if(compareSensorAddresses(sensorAddrs[i], iter->addr)) {
        found = true;
      }
    }

    if(found) {
      Serial.println("Sensor Already Found");
      sensor_id++;
      continue;
    }

    Sensor sensor = Sensor(ds);

    type_s = chipType(sensorAddrs[i][0]);

    sensor.id = sensor_id;
    sensor.type = type_s;

    Serial.print("SensorID: ");
    Serial.print(sensor.id);
    Serial.print("    Sensor Type: ");
    Serial.println(sensor.type);

    memcpy(&sensor.addr, &sensorAddrs[i], SENSOR_ADDR_SIZE);

    sensors.push_front(sensor);

    sensor_id++;
  }

  Serial.println("Removing Missing Sensors");
  for(list<Sensor>::iterator it=sensors.begin(); it != sensors.end(); ++it) {
    if(it->id == 0) continue;

    found = false;
    for(int i = 0; i < MAX_SENSORS; i++) {
      if(isByteArrayEmpty(sensorAddrs[i])) continue;

      if(compareSensorAddresses(sensorAddrs[i], it->addr)) {
        found = true;
      }
    }

    if(found) {
      continue;
    }

    Serial.println("Found a Sensor to Remove");
    sensors.remove(*it);
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

int Sensors::count() {
  return sensors.size();
}
