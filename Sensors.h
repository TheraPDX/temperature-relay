#ifndef Sensors_h
#define Sensors_h
#endif

#include "OneWire.h"
#include <list>
using namespace std;

#define SENSOR_ADDR_SIZE 8

class Sensor {
  OneWire & ds;
  list<float> temperatures;

  float averageTemperatures();

  public:
    int id = 0;
    byte addr[SENSOR_ADDR_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0};
    byte type = '\0';
    float temp;
    float minute_average;

    Sensor(OneWire &ds): ds(ds) {
      list<float> temps(6);
      temperatures = temps;
    };
    void read();
};

class Sensors {
  list<Sensor> sensors;
  OneWire & ds;

  float averageTemperatures();

  public:
    Sensors(OneWire &ds): ds(ds) {};
    void scan();
    void read();
    void debug();
    float minute_average;
};
