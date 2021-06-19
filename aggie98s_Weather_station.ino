/*  
6/19/2021 - Initial commit.  Code is missing sensors for Rain, Solar, UV, and Wind but will update MQTT and Wunderground while utilizing Deep Sleep




*/

#include <OneWire.h>           // https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h> // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <stdlib.h>
#include <math.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "time.h"
// #include <task.h>

#define uS_TO_S_FACTOR 1000000  //Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  30          //Time ESP32 will go to sleep (in seconds)
#define ONE_WIRE_BUS 4             // The pin temperature  DS18B20 sensor

//Define MQTT and Topics
#define mqtt_server "172.31.11.103"
#define mqtt_user "mqttuser"
#define mqtt_password "mqttpass"
#define humidity_topic "sensor/humidity"
#define temperature_topic "sensor/temperature"
#define dewpoint_topic "sensor/dewpoint"
#define windSpeed_topic "sensor/windSpeed"
#define wind_avg_topic "sensor/wind_avg"
#define windgustmph_topic "sensor/windgustmph"
#define barompa_topic "sensor/barompa"
#define rain1h_topic "sensor/rain1h"
#define rain24h_topic "sensor/rain24h"
#define uvIntensity_topic "sensor/uvIntensity"
#define UVmax_topic "sensor/UVmax"
#define solar_topic "sensor/solar"
#define updatetime_topic "sensor/updatetime"

/*  Variables */ 
RTC_DATA_ATTR float tempc;             // Temp celsius DS18B20
//RTC_DATA_ATTR float tempc_min= 100;    // Minimum temperature C
//RTC_DATA_ATTR float tempc_max;         // Maximum temperature C
RTC_DATA_ATTR float temp2c;            // Temp celsius  BMP280
RTC_DATA_ATTR float humidity;          // Humidity BMP280
RTC_DATA_ATTR float humid = 0;         // Humidity variable
RTC_DATA_ATTR float tempf=0;           // Temp farenheit BMP280
RTC_DATA_ATTR float internalTempf=0;   // Float for AHT Conversion to Farenheit
RTC_DATA_ATTR float internalTempc=0;   // Float for AHT Conversion to Celsius
RTC_DATA_ATTR float dewptf=0;          // dew point tempf
//RTC_DATA_ATTR float dewptc=0;          // dew point tempc
RTC_DATA_ATTR float windSpeed;         // Wind speed (mph)
RTC_DATA_ATTR float wind_min = 100;     // Minimum wind speed (mph)
RTC_DATA_ATTR float wind_avg;          // 10 minutes average wind speed ( mph)
RTC_DATA_ATTR float windgustmph = 0;   // Maximum Wind gust speed( mph)
RTC_DATA_ATTR float barompa;           // Preasure pascal Pa
RTC_DATA_ATTR float baromin;           // Preasure inches of mercury InHg
RTC_DATA_ATTR float baromhpa;          // Preasure hectopascal hPa
RTC_DATA_ATTR float rain1h = 0;        // Rain inches over the past hour
RTC_DATA_ATTR float rain24h = 0;       // Rain inches over the past 24 hours
RTC_DATA_ATTR float uvIntensity = 0;   // UV 
RTC_DATA_ATTR float UVmax = 0;         // maximum UV intensity over the past 10 minutes
RTC_DATA_ATTR int solar  = 0;          // solarradiation - [W/m^2]
RTC_DATA_ATTR float altitudepws = 30.00;  //Local  altitude of the PWS to get relative pressure  meters (m)
RTC_DATA_ATTR bool firsttime = true;
RTC_DATA_ATTR int bootCount = 0;

const char* serverName = "http://rtupdate.wunderground.com";  // Wunderground Update Server
const char* ntpServer = "us.pool.ntp.org";
const long  gmtOffset_sec = -21600;   //-6hrs gmt offset
const int   daylightOffset_sec = 3600;
char TIMESTAMP;



// Credentials.
char ssid[] = "SSID";
char pass[] = "Wifipassword";
char ID [] = "Station ID";          // ID of my PWS
char PASSWORD [] = "Station Key";        // Key for PWS


WiFiClient espClient;               //Setup WiFiClient
PubSubClient client(espClient);     //Configure MQTT
OneWire oneWire(ONE_WIRE_BUS);          // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
Adafruit_AHTX0 aht;                     //Setup ATH10 Humidity Sensor
DallasTemperature DS18B20(&oneWire);    // Pass our oneWire reference to Dallas Temperature.
TaskHandle_t wundergroundTask;        // Create Task Handle for publishing ot Wunderground




//========================= Setup Function ======================================
void setup() {
  Serial.begin(115200);                       //Set serial communication
  bootCount++;
  DS18B20.begin();                            //Start External Temperature Sensor
  aht.begin();                                // Start Humidity Sensor
  WiFi.begin(ssid, pass);                     // Connect to WiFI
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
//    Serial.println("Connecting to WiFi..");
  }
//   Serial.println("Connected to the WiFi network");
  client.setServer(mqtt_server, 1883);        //Set MQTT Server
  client.setKeepAlive(120);                   //Set Keep Alive for MQTT Connection to 120 seconds so we don't have to keep reconnecting.
  if (!client.connected()) {                  //If MQTT Client isn't connected, reconnect
    reconnect();
  }
  client.loop();
 
  readSensors();                                                     //Read Sensors
//  display_to_console();                                            // Display data to Serial Console
if (tempf < -50 or firsttime) {
  Serial.println("Temp below threshold or first time.  Skip publishing to Wunderground amd MQTT");
  firsttime = false;
}
else {
//  wunderground();                                                    //Publish to Wunderground
  publishMQTT();                                                     //Publish to MQTT
}
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);     //Set timer to 5 seconds
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  " Seconds");

  esp_deep_sleep_start();   //Go to sleep

}

//END SETUP

// Main Loop has no statements since everything in setup due to deep sleep
void loop() {}

//Attempts to reconnect to MQTT;  Should only execute once when ESP32 first starts up since
//KeepAlive is set at 120 seconds and we loop every 60 seconds
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
//    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
//      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}  //End reconnect

void readSensors(void) {
  DS18B20.requestTemperatures();
  tempc = DS18B20.getTempCByIndex(0);
  tempf = DS18B20.getTempFByIndex(0);
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp); // populate temp and humidity
  humid = humidity.relative_humidity;
  internalTempc = temp.temperature;
  internalTempf = temp.temperature;
  internalTempf = internalTempf*1.8;
  internalTempf = internalTempf + 32;
  dewptf = (dewPoint(tempf, humidity.relative_humidity));   
// Force other sensors for testing
  windSpeed = .5;
  wind_avg = .1;
  windgustmph = .15;
  barompa = 1200;
  rain1h = .02;
  rain24h = .44;
  uvIntensity = 5;
  UVmax = 7;
  solar = 1000;
  
}   //End readSensors

//-------------------------------------------------------------------
// Following function sends sensor data to wunderground.com
// This string works: 
// http://rtupdate.wunderground.com/weatherstation/updateweatherstation.php?ID=KTXSANAN1884&PASSWORD=0bwqjKrP&dateutc=now&winddir=270&windspeedmph=7.0humidity=45.4&tempf=43.1&baromin=29.4161&realtime=1&rtfreq=10&action=updateraw 
//
//
//-------------------------------------------------------------------

void wunderground(void)
{ 
// My Wunderground string
// Build string to publish to Wunderground  
//Serial.print("wunderground() running on core ");
//Serial.println(xPortGetCoreID());
  Serial.println("Publishing to Wunderground");
  String cmd = serverName;
  cmd += "/weatherstation/updateweatherstation.php?ID=";
  cmd += ID;
  cmd += "&PASSWORD=";
  cmd += PASSWORD;
  cmd += "&dateutc=now";
//  cmd += "&winddir=";
//  cmd += CalDirection;
  cmd += "&windspeedmph=";
  cmd += wind_avg;    
  cmd += "&windgustmph=";
  cmd += windgustmph;
  cmd += "&tempf=";
  cmd += tempf;
  cmd += "&dewptf=";
  cmd += dewptf;
  cmd += "&humidity=";
  cmd += humid;
  cmd += "&baromin=";
  cmd += baromin;
  cmd += "&solarradiation=";
  cmd += solar;
  cmd += "&UV=";
  cmd += UVmax;
  cmd += "&rainin=";
  cmd += rain1h;
  cmd += "&dailyrainin=";
  cmd += rain24h;
  cmd += "&softwaretype=Arduino-ESP32&action=updateraw&realtime=1&rtfreq=30";
  if ((WiFi.status() == WL_CONNECTED)) {                    //Check the current connection status
       HTTPClient http;
       http.begin(cmd);
//       Serial.println(cmd);
       int httpCode = http.GET();                             //Publish to Wunderground
     if (httpCode > 0) { //Check for the returning code
        String payload = http.getString();
//        Serial.println(httpCode);
        Serial.println(payload);
      }
    else {
      Serial.println("Error on HTTP request");
    }
     http.end(); //Free the resources
  }
}  //End of wunderground

void publishMQTT(void) {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  // Get Time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
    char timeDayOfWeek[10];
    char timeMonth[10];
    char timeDay[3];
    char timeYear[5];
    char timeHour[3];
    char timeMinute[3];
    char timeSecond[3];    
    strftime(timeDayOfWeek,10, "%A", &timeinfo);
    strftime(timeMonth,10, "%B", &timeinfo);
    strftime(timeDay,3, "%d", &timeinfo);
    strftime(timeYear,5, "%Y", &timeinfo);
    strftime(timeHour,3, "%H", &timeinfo);
    strftime(timeMinute,3, "%M", &timeinfo);
    strftime(timeSecond,3, "%S", &timeinfo);
    String datetime = timeMonth;
     datetime += ' ';
     datetime += timeDay;
     datetime += ',';
     datetime += timeYear;
     datetime += ' ';
     datetime += timeHour;
     datetime += ':';
     datetime += timeMinute;
     datetime += ':';
     datetime += timeSecond;
      Serial.println("Publishing to MQTT");
      client.publish(updatetime_topic, String(datetime).c_str(), true);
      client.publish(temperature_topic, String(tempf).c_str(), true);
      client.publish(humidity_topic, String(humid).c_str(), true);
      client.publish(dewpoint_topic, String(dewptf).c_str(), true);
      client.publish(windSpeed_topic, String(windSpeed).c_str(), true);
      client.publish(wind_avg_topic, String(wind_avg).c_str(), true);   
      client.publish(windgustmph_topic, String(windgustmph).c_str(), true);
      client.publish(barompa_topic, String(barompa).c_str(), true);
      client.publish(rain1h_topic, String(rain1h).c_str(), true);
      client.publish(rain24h_topic, String(rain24h).c_str(), true);
      client.publish(uvIntensity_topic, String(uvIntensity).c_str(), true);
      client.publish(UVmax_topic, String(UVmax).c_str(), true);
      client.publish(solar_topic, String(solar).c_str(), true);   
}

//------------------------------------------------------------------------------------------------------------
///////////////////////////////// Dew point calculation//////////////////////////////////////////////////////
//------------------------------------------------------------------------------------------------------------

double dewPoint(double tempf, double humidity)
{
        double RATIO = 373.15 / (273.15 + tempf);  // RATIO was originally named A0, possibly confusing in Arduino context
        double SUM = -7.90298 * (RATIO - 1);
        SUM += 5.02808 * log10(RATIO);
        SUM += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
        SUM += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
        SUM += log10(1013.246);
        double VP = pow(10, SUM - 3) * humidity;
        double T = log(VP/0.61078);   // temp var
        return (241.88 * T) / (17.558 - T);
}
