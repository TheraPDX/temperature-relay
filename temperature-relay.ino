#include "OneWire.h"
#include "WebServer.h"
#include "elapsedMillis.h"
#include <list>
#include "Sensors.h"

using namespace std;

#define SCAN_INTERVAL 60000
#define TEMP_INTERVAL 10000
#define BLINK_INTERVAL 5000
#define POWER_INTERVAL 120000
#define PREFIX ""

template<class T>
inline Print &operator <<(Print &obj, T arg)
{ obj.print(arg); return obj; }

elapsedMillis scanTimeElapsed;
elapsedMillis tempTimeElapsed;
elapsedMillis blinkTimeElapsed;
elapsedMillis powerTimeElapsed; 

OneWire ds(D1);
Sensors sensors(ds);
WebServer webserver(PREFIX, 80);

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

void helloCmd(WebServer &server, WebServer::ConnectionType type, char *, bool){
  /* server.httpSuccess(); */
  server.httpSuccess("text/plain; version=0.0.4");
  if (type != WebServer::HEAD) {
    char s_temp[85];
    char s_average_temp[110];
    char s_power[50];
    char s_freemem[110];
    uint32_t freemem;

    freemem = System.freeMemory();

    snprintf(s_temp, 85,"# TYPE temp_degrees gauge\ntemp_degrees %.4f %li000\n\n", temperature, Time.now());
    snprintf(s_average_temp, 110, "# TYPE average_temp_degrees gauge\naverage_temp_degrees %.4f %li000\n\n", minuteAverage, Time.now());
    snprintf(s_power, 50, "# TYPE heater gauge\nheater %i %li000\n\n", power, Time.now());
    snprintf(s_freemem, 110, "# Type free_mem_bytes\nfree_mem_bytes %lu %li000\n\n", freemem, Time.now());

    server << s_temp;
    server << s_average_temp;
    server << s_power;
    server << s_freemem;
  }
}


void setup(void) {
  Serial.begin(57600);
  
  Particle.variable("power", power);
  Particle.variable("temperature", temperature);
  Particle.variable("min_average", minuteAverage);
  Particle.variable("threshold", tempThreshold);
  Particle.function("power", adjustPower);
  publishPowerStatus();

  webserver.setDefaultCommand(&helloCmd);
  webserver.addCommand("metrics", &helloCmd);
  webserver.begin();

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
  
  snprintf(publishString, 20,"%.4f", temp);
  Particle.publish("temperature", publishString);
  Serial.print("Temp: ");
  Serial.println(publishString);
}

void publishAverage(float temp) {
  char publishString[20];
  
  snprintf(publishString, 20,"%.4f", temp);
  Particle.publish("minute_average", publishString);
  Serial.print("Average: ");
  Serial.println(publishString);
}

void debugPublish(const char message[]) {
  Particle.publish("debug", message);
  Serial.println(message);
}

void loop(void) {
  char buff[64];
  int len = 64;
  webserver.processConnection(buff, &len);

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
    if(sensors.count() > 0) {
      sensors.read();

      minuteAverage = sensors.minute_average;
      temperature = sensors.temp;

      Serial.print("Temp: ");
      Serial.println(temperature);
      Serial.print("Average Temp: ");
      Serial.println(minuteAverage);

      publishAverage(minuteAverage);
      publishTemp(temperature);
    } else {
      Serial.println("No Sensors to Read");
    }
  }

  if (powerTimeElapsed > POWER_INTERVAL) {
    powerTimeElapsed = 0;

    Serial.println("## Evaluating Power");

    if(minuteAverage > tempThreshold) {
      turnOffPower();
    } else {
      turnOnPower();
    }
  }
}
