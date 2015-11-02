#ifndef Sensor_h
#define Sensor_h

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

#endif
