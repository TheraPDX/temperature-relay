#ifndef Sensors_h
#define Sensors_h

#include "OneWire.h"
#include "Sensor.h"
#include <list>
using namespace std;

class Sensors {
  list<Sensor> sensors;
  OneWire & ds;

  float averageTemperatures();
  float minuteAverageTemperatures();

  public:
    Sensors(OneWire &ds): ds(ds) {};
    void scan();
    void read();
    void debug();
    float temp;
    float minute_average;
};

#endif
