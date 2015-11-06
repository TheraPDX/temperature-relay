#ifndef SENSORS_H_
#define SENSORS_H_

#include "OneWire.h"
#include "Sensor.h"
#include <list>

#define MAX_SENSORS 10

class Sensors {
  OneWire & ds;
  std::list<Sensor> sensors;

  float averageTemperatures();
  float minuteAverageTemperatures();

  public:
    Sensors(OneWire &ds): ds(ds) {}
    void scan();
    void read();
    void debug();
    int count();
    float temp;
    float minute_average;
};

#endif // SENSORS_H_
