#include "OneWire.h"
#include "WebServer.h"
#include "HttpClient.h"
#include "elapsedMillis.h"
#include <list>
#include <string>
#include "Sensors.h"
#include "ApiKeys.h"

using namespace std;

#define SCAN_INTERVAL 60000
#define TEMP_INTERVAL 10000
#define BLINK_INTERVAL 5000
#define POWER_INTERVAL 120000
#define WEATHER_INTERVAL 120000
#define PREFIX ""
#define WEATHER_ZIP "68522"

template<class T>
inline Print &operator <<(Print &obj, T arg){
  obj.print(arg);
  return obj;
}

elapsedMillis scanTimeElapsed;
elapsedMillis tempTimeElapsed;
elapsedMillis blinkTimeElapsed;
elapsedMillis powerTimeElapsed;
elapsedMillis weatherTimeElapsed;

OneWire ds(D1);
Sensors sensors(ds);
WebServer webserver(PREFIX, 80);
HttpClient http;

http_header_t headers[] = {
  { "User-Agent", "curl/7.43.0"},
  { "Accept", "*/*"},
  { NULL, NULL }
};
http_request_t request;
http_response_t response;

int led1 = D7;
int powertail = D5;

double temperature;
double minuteAverage;
double outdoorTemp;
double tempOnThreshold = 60.0;
double tempOffThreshold = 65.0;
int power = 0;
boolean ledState = LOW;

void startup() {
  pinMode(led1, OUTPUT);
  pinMode(powertail, OUTPUT);
}

STARTUP( startup() );

void metricsCmd(WebServer &server, WebServer::ConnectionType type, char *, bool){
  server.httpSuccess("text/plain; version=0.0.4");
  if (type != WebServer::HEAD) {
    char s_temp[100];
    char s_average_temp[100];
    char s_outdoor_temp[100];
    char s_power_on_temp[100];
    char s_power_off_temp[100];
    char s_power[25];
    char s_freemem[90];
    uint32_t freemem;

    freemem = System.freeMemory();

    server << "# TYPE temp_degrees gauge\n";
    snprintf(s_temp, 100,"temp_degrees{location=\"garage\",timespan=\"none\"} %.4f %li000\n", temperature, Time.now());
    snprintf(s_average_temp, 100, "temp_degrees{location=\"garage\",timespan=\"minute\"} %.4f %li000\n", minuteAverage, Time.now());
    snprintf(s_outdoor_temp, 100, "temp_degrees{location=\"outdoors\",timespan=\"none\"} %.4f %li000\n", outdoorTemp, Time.now());
    snprintf(s_power_on_temp, 100, "temp_degrees{trigger=\"on\",timespan=\"none\"} %.4f %li000\n", tempOnThreshold, Time.now());
    snprintf(s_power_off_temp, 100, "temp_degrees{trigger=\"off\",timespan=\"none\"} %.4f %li000\n\n", tempOffThreshold, Time.now());

    server << s_temp;
    server << s_average_temp;
    server << s_outdoor_temp;
    server << s_power_on_temp;
    server << s_power_off_temp;

    server << "# TYPE heater gauge\n";
    snprintf(s_power, 25, "heater %i %li000\n\n", power, Time.now());

    server << s_power;

    server << "# Type free_mem_bytes\n";
    snprintf(s_freemem, 90, "free_mem_bytes %lu %li000\n\n", freemem, Time.now());

    server << s_freemem;
  }
}


void setup(void) {
  Serial.begin(57600);

  Particle.variable("power", power);
  Particle.variable("temperature", temperature);
  Particle.variable("min_average", minuteAverage);
  Particle.variable("temp_on", tempOnThreshold);
  Particle.variable("temp_off", tempOffThreshold);

  Particle.function("power", adjustPower);
  Particle.function("setTempOn", setTempOn);
  Particle.function("setTempOff", setTempOff);

  publishPowerStatus();

  webserver.setDefaultCommand(&metricsCmd);
  webserver.addCommand("metrics", &metricsCmd);
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

int setTempOn(String temp) {
  float f_temp = temp.toFloat();
  if(f_temp > 0 && f_temp < tempOffThreshold) {
    tempOnThreshold = f_temp;
    return 1;
  } else {
    return -1;
  }
}

int setTempOff(String temp) {
  float f_temp = temp.toFloat();
  if(f_temp > 0 && f_temp > tempOnThreshold) {
    tempOffThreshold = f_temp;
    return 1;
  } else {
    return -1;
  }
}

void turnOnPower() {
  if(power != 1) {
    power = 1;
    digitalWrite(powertail, HIGH);
    publishPowerStatus();
  }
}

void turnOffPower() {
  if(power != 0) {
    power = 0;
    digitalWrite(powertail, LOW);
    publishPowerStatus();
  }
}

void publishPowerStatus() {
  if(power == 1) {
    Particle.publish("power", "On");
  } else {
    Particle.publish("power", "Off");
  }
}

void publishTemp(const char pLabel[], const char sLabel[], float temp) {
  char publishString[20];

  snprintf(publishString, 20,"%.4f", temp);
  Particle.publish(pLabel, publishString);
  Serial.print(sLabel);
  Serial.println(publishString);
}

void debugPublish(const char message[]) {
  Particle.publish("debug", message);
  Serial.println(message);
}

String tryExtractString(String str, const char* start, const char* end) {
  if (str == NULL) {
    return NULL;
  }

  int idx = str.indexOf(start);
  if (idx < 0) {
    return NULL;
  }

  int endIdx = str.indexOf(end);
  if (endIdx < 0) {
    return NULL;
  }

  return str.substring(idx + strlen(start), endIdx);
}

void fetchWeather() {
  String body;
  String tempStr;
  float f_temp;

  Serial.println("Fetching Weather");

  request.hostname = "api.openweathermap.org";
  request.port = 80;
  request.path="/data/2.5/weather?zip=" WEATHER_ZIP ",us&units=imperial&appid=" OPENWEATHERMAP_API_KEY;
  http.get(request, response, headers);

  Particle.publish("Weather HTTP Code", String(response.status));
  Serial.println(response.status);
  Serial.println("### HTTP BODY START ###");
  Serial.println(response.body);
  Serial.println("### HTTP BODY END ###");

  body = String(response.body);
  tempStr = tryExtractString(body, "\"main\":{\"temp\":",",\"pressure\":");

  if (tempStr != NULL) {
    f_temp = tempStr.toFloat();
    if (f_temp !=0) {
      outdoorTemp = (double)f_temp;
      publishTemp("outdoor_temp", "Outdoor Temp: ", outdoorTemp);
    }
  }
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

      minuteAverage = (double)sensors.minute_average;
      temperature = (double)sensors.temp;

      Serial.print("Temp: ");
      Serial.println(temperature);
      Serial.print("Average Temp: ");
      Serial.println(minuteAverage);

      publishTemp("minute_average", "Average Temp: ", minuteAverage);
      publishTemp("temperature", "Temp: ", temperature);
    } else {
      Serial.println("No Sensors to Read");
    }
  }

  if (powerTimeElapsed > POWER_INTERVAL) {
    powerTimeElapsed = 0;

    Serial.println("## Evaluating Power");

    if(minuteAverage > tempOffThreshold) {
      turnOffPower();
    } else if(minuteAverage < tempOnThreshold) {
      turnOnPower();
    }
  }

  if (weatherTimeElapsed > WEATHER_INTERVAL) {
    weatherTimeElapsed = 0;
    fetchWeather();
  }
}
