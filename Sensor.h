#ifndef Sensor_h
#define Sensor_h

#include "OneWire.h"
#include <list>
using namespace std;

#define SENSOR_ADDR_SIZE 8

bool compareSensorAddresses(const byte lhs[8], const byte rhs[8]);

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

    bool operator==(const Sensor& rhs) {
      return compareSensorAddresses(addr, rhs.addr);
    }

    void read();
};

#endif
