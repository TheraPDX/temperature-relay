#include "OneWire.h"
#include "elapsedMillis.h"
#include <list>
#include "Sensors.h"

using namespace std;

#define SCAN_INTERVAL 60000
#define TEMP_INTERVAL 10000
#define BLINK_INTERVAL 5000
#define POWER_INTERVAL 120000

elapsedMillis scanTimeElapsed;
elapsedMillis tempTimeElapsed;
elapsedMillis blinkTimeElapsed;
elapsedMillis powerTimeElapsed; 

OneWire ds(D1);
Sensors sensors(ds);

int led1 = D7;
int powertail = D5;

float temperature;
float minuteAverage;
float tempThreshold = 62.0F;
bool power = false;
boolean ledState = LOW;

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

  sensors.scan();
  sensors.debug();
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

void loop(void) {
  if (blinkTimeElapsed > BLINK_INTERVAL) {
    ledState = !ledState; 
    digitalWrite(led1, ledState);
    blinkTimeElapsed = 0;
  }
  
  if (scanTimeElapsed > SCAN_INTERVAL) {
    scanTimeElapsed = 0;

    Serial.println("## Scanning for Sensors");
    sensors.scan();

    sensors.debug();
  }

  if (tempTimeElapsed > TEMP_INTERVAL) {
    tempTimeElapsed = 0;

    Serial.println("## Reading Temps");
    sensors.read();

    Serial.print("Average Temp: ");
    Serial.println(sensors.minute_average);
    publishAverage(sensors.minute_average);
    publishTemp(sensors.temp);
  }

  if (powerTimeElapsed > POWER_INTERVAL) {
    powerTimeElapsed = 0;

    Serial.println("## Evaluating Power");

    if(sensors.minute_average > tempThreshold) {
      turnOffPower();
    } else {
      turnOnPower();
    }
  }
}
