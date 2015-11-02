#include "OneWire.h"
#include "elapsedMillis.h"
#include <list>

using namespace std;

#define SENSOR_ADDR_SIZE 8
#define SCAN_INTERVAL 60000
#define TEMP_INTERVAL 10000
#define BLINK_INTERVAL 5000
#define POWER_INTERVAL 120000

elapsedMillis scanTimeElapsed;
elapsedMillis tempTimeElapsed;
elapsedMillis blinkTimeElapsed;
elapsedMillis powerTimeElapsed; 

OneWire ds(D1);
int led1 = D7;
int powertail = D5;

float temperature;
float minuteAverage;
list<float> temperatures(6);
float tempThreshold = 62.0F;
bool power = false;
boolean ledState = LOW;

struct Sensor {
  int id = 0;
  byte addr[SENSOR_ADDR_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0};
  byte type = '\0';
};

Sensor sensors[5];
int num_sensors = sizeof(sensors)/sizeof(Sensor);

void startup() {
  pinMode(led1, OUTPUT);
  pinMode(powertail, OUTPUT);
}

STARTUP( startup() );

void setup(void) {
  Serial.begin(57600);
  
  Particle.variable("power", power);
  Particle.variable("temperature", temperature);
  Particle.variable("min_average", minuteAverage);
  Particle.variable("threshold", tempThreshold);
  Particle.function("power", adjustPower);
  publishPowerStatus();

  findSensors(sensors, num_sensors);
}

int adjustPower(String command) {
  if(command == "on") {
    turnOnPower();
  }
  if(command == "off") {
    turnOffPower();
  }
  return 1;
}

void turnOnPower() {
  if(power != true) {
    power = true;
    digitalWrite(powertail, HIGH);
    publishPowerStatus();
  }
}

void turnOffPower() {
  if(power != false) {
    power = false;
    digitalWrite(powertail, LOW);
    publishPowerStatus();
  }
}

void publishPowerStatus() {
  if(power == true) {
    Particle.publish("power", "On");
  } else {
    Particle.publish("power", "Off");
  }
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
    default:
      debugPublish("Device is not a DS18x20 family device.");
  }
  return type_s;
}

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

void publishTemp(float temp) {
  char publishString[20];
  
  snprintf(publishString, 20,"%f", temp);
  Particle.publish("temperature", publishString);
  Serial.print("Temp: ");
  Serial.println(publishString);
}

void publishAverage(float temp) {
  char publishString[20];
  
  snprintf(publishString, 20,"%f", temp);
  Particle.publish("minute_average", publishString);
  Serial.print("Average: ");
  Serial.println(publishString);
}

void debugPublish(const char message[]) {
  Particle.publish("debug", message);
  Serial.println(message);
}

bool findAndValidateDeviceAddress(uint8_t *addr) {
  bool success = true;
  if ( !ds.search(addr)) {
    ds.reset_search();
    delay(250);
    success = false;
  }
  
  if (success && OneWire::crc8(addr, 7) != addr[7]) {
    debugPublish("CRC is not valid!");
    success = false;
  }
  return success;
}

bool readData(byte data[12]) {
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

void findSensors(struct Sensor *sensors, int num_sensors) {
  Serial.println("## Scanning for Sensors ##");
  byte type_s;
  byte addr[8];
  int sensor_id = 0;

  while(findAndValidateDeviceAddress(addr) && sensor_id < num_sensors) {
    type_s = chipType(addr[0]);
    sensors[sensor_id].id = sensor_id + 1;
    sensors[sensor_id].type = type_s;
    memcpy(&sensors[sensor_id].addr, &addr, SENSOR_ADDR_SIZE);

    const char* name = getChipName(sensors[sensor_id].type);
    char sensorMessage[75];
    snprintf(
      sensorMessage,
      75,
      "Sensor #%i, Type: %i - %s, Address: %x %x %x %x %x %x %x %x",
      sensors[sensor_id].id,
      sensors[sensor_id].type,
      name,
      sensors[sensor_id].addr[0],
      sensors[sensor_id].addr[1],
      sensors[sensor_id].addr[2],
      sensors[sensor_id].addr[3],
      sensors[sensor_id].addr[4],
      sensors[sensor_id].addr[5],
      sensors[sensor_id].addr[6],
      sensors[sensor_id].addr[7]
    );
    Serial.println(sensorMessage);

    sensor_id++;
  }
  ds.reset();
}

float averageTemperatures(list<float> temperatures) {
  float accum = 0;
  int count = 0;
  float result = 0;

  for (list<float>::iterator it=temperatures.begin(); it != temperatures.end(); ++it) {
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

void readTemperatures(struct Sensor *sensors, int num_sensors) {
  Serial.println("## Reading Temperatures ##");
  byte present = 0;
  byte data[12];
  float celsius, fahrenheit;

  for ( int i = 0; i < num_sensors; i++) {
    if (sensors[i].id == 0) continue;

    ds.select(sensors[i].addr);
    ds.write(0x44);        // start conversion, with parasite power on at the end
    delay(900);     // maybe 750ms is enough, maybe not
    // we might do a ds.depower() here, but the reset will take care of it.
    
    present = ds.reset();
    ds.select(sensors[i].addr);
    ds.write(0xBE, 0);         // Read Scratchpad
    
    if(!readData(data)){
      debugPublish("Invalid Data CRC");
      continue;
    }
    
    celsius = parseTempValue(data, sensors[i].type);
    fahrenheit = celsius * 1.8 + 32.0;
    
    if(fahrenheit < (float)-30 || fahrenheit > (float)120) {
      char error[40];
      snprintf(error, 40, "Invalid Temperature: %f", fahrenheit);
      debugPublish(error);
      
      continue;
    }
    temperature = fahrenheit;
    temperatures.erase(temperatures.begin());
    temperatures.push_back(fahrenheit);
    minuteAverage = averageTemperatures(temperatures);
    
    publishTemp(fahrenheit);
    publishAverage(minuteAverage);
  }
}

void loop(void) {
  if (blinkTimeElapsed > BLINK_INTERVAL) {
    ledState = !ledState; 
    digitalWrite(led1, ledState);
    blinkTimeElapsed = 0;
  }
  
  if (scanTimeElapsed > SCAN_INTERVAL) {
    scanTimeElapsed = 0;
    findSensors(sensors, num_sensors);
  }

  if (tempTimeElapsed > TEMP_INTERVAL) {
    tempTimeElapsed = 0;
    readTemperatures(sensors, num_sensors);
  }

  if (powerTimeElapsed > POWER_INTERVAL) {
    powerTimeElapsed = 0;

    if(minuteAverage > tempThreshold) {
      turnOffPower();
    } else {
      turnOnPower();
    }
  }
}
