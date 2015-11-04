#ifndef Sensors_h
#define Sensors_h

#include "OneWire.h"
#include "Sensor.h"
#include <list>

#define MAX_SENSORS 10

using namespace std;

class Sensors {
  OneWire & ds;
  list<Sensor> sensors;

  float averageTemperatures();
  float minuteAverageTemperatures();

  public:
    Sensors(OneWire &ds): ds(ds) {};
    void scan();
    void read();
    void debug();
    int count();
    float temp;
    float minute_average;
};

#endif
