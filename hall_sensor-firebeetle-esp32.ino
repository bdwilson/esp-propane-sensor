// Propane Monitor with FireBeetle ESP32
// 
// Firebeetle was picked because of it's low power usage in deepsleep mode.
// Current settings wake this up every hour to see if there is an update
// in either voltage or propane percentage, and if so, it sends an update.
// An update is forced every 4th run (1 hour) even if there are no updates so
// that you can track "LastUpdated" time to make sure it's not dead.
// 
// I have no idea how long the battery will last; will update when I know more.
// 
// Shoutouts:
// * https://github.com/Torxgewinde/Firebeetle-2-ESP32-E
// * https://forums.homeseer.com/forum/legacy-software-plug-ins/legacy-plug-ins/legacy-homeseer-plug-ins/adi-ocelot/53385-propane-level-monitoring-with-rd3-hall-effect-sensor/page2
// * https://www.vboxmotorsport.co.uk/index.php/us/calculators
// * https://diyi0t.com/reduce-the-esp32-power-consumption/ 

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "esp_adc_cal.h"

// these aren't needed if you don't enable BTLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#define SERVICE_UUID        "25596671-5116-40a4-934f-802cf6958d8c"

// which analog pin to connect hall sensor
#define ANALOGPIN A1
// wifi information
#define ESSID "myssid"
#define PSK "mypassword" 
// number of samples to take each reading when reading the hall sensor. 
#define NUMSAMPLES 3
// between 20% (650) and 80% (2950)
// these are my calibrations against a Tank Utility hall sensor
// and an official Rochester 
// https://www.vboxmotorsport.co.uk/index.php/us/calculators
#define SCALE 0.02608695652173913
#define OFFSET 3.0434782608695663
#define ADD .5
// deep sleep time
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
//#define TIME_TO_SLEEP  10 /* Time ESP32 will go to sleep (in seconds) */
#define TIME_TO_SLEEP  10800 /* Time ESP32 will go to sleep (in seconds) - 3 hrs */

// genmon/tank info info
bool doGenmon = 1; // set to 0 if you don't use genmon.
const uint16_t port = 9082;
const char * host = "192.168.1.58";
String tankname = "External Tank";
String capacity = "320";

// hubitat info
bool doHubitat = 1;  // set to 0 if you don't use hubitat.
String HubitatApp = "http://192.168.1.xx/apps/api/1234/update";
String HubitatAccess = "15f62234-xxxx-xxxxx-xxxx-xxxxx˝;

// enable BLE name: PROPANE:LEVEL=xxx:BATT=xxx
bool doBLE = 0;

// don't change these. 
String percentage = "";
bool isSent = 0;
double sensorValue = 0;
bool doUpdate = 0; 
bool doPropane = 0; 
bool doWifi = 0;

WiFiClient client;
HTTPClient http;

RTC_NOINIT_ATTR struct {
  uint8_t bssid[6];
  uint8_t channel;

  double BatteryVoltage;      //battery voltage in V
  uint64_t NumberOfRestarts; //number of restarts
  uint64_t ActiveTime;       //time being active in ms
  double tankPercentage;         //hall sensor. 
} cache;

void setup() {
  //visual feedback when we are active, turn on onboard LED
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  //char oneDigit[4];
  
  cache.NumberOfRestarts++;

  Serial.begin(115200);
  Serial.print("===================================================\r\n");
  Serial.printf("FireBeetle active\r\n" \
                " Compiled at: " __DATE__ " - " __TIME__ "\r\n" \
                " ESP-IDF: %s\r\n", esp_get_idf_version());
  Serial.print("Number of restarts: ");
  Serial.println(cache.NumberOfRestarts);
  //read battery voltage
  sensorValue = getSample();
  delay(500);
  if ((int)sensorValue != (int)cache.tankPercentage) {
    Serial.print("Sensor Value changed from ");
    Serial.print(cache.tankPercentage);
    Serial.print(" to ");
    Serial.println(sensorValue);
    //Serial.println(sensorValue);
    cache.tankPercentage = sensorValue;
    doPropane=1;
  }

  int count = (int(cache.NumberOfRestarts)%4);
  Serial.print("Num restarts mod 4: ");
  Serial.println(count);
  Serial.println("....");
 if (doPropane || (count == 0)) {
    double BatteryVoltage = readBattery();
    delay(500);
    if (BatteryVoltage != cache.BatteryVoltage) {
       Serial.print("Battery voltage changed from ");
       Serial.print(cache.BatteryVoltage); 
       Serial.print(" to ");
       Serial.println(BatteryVoltage);
       cache.BatteryVoltage=BatteryVoltage;
       doUpdate=1;
    }
  }

  // force an update every 4 restarts
  // so if you sleep for 3 hrs, force an update every 12
  if ((count == 0) && !doUpdate) {
    Serial.print("doPropane: ");
    Serial.println(doPropane);
    Serial.print("doUpdate: ");
    Serial.println(doUpdate);
    Serial.print("restarts: ");
    Serial.println(cache.NumberOfRestarts);
    Serial.println("No update for awhile. Forcing update.");
    doUpdate=1;
  }
  
  if (doHubitat || doGenmon) {
    doWifi = 1;
  }
  
  //check if a reset/power-on occured
  if (esp_reset_reason() == ESP_RST_POWERON) {
      Serial.println("ESP was just switched ON");
      cache.ActiveTime = 0;
      cache.NumberOfRestarts = 0;

      //set RTC to 0
      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);

      if (doWifi) {
        //default is to have WiFi off
        if (WiFi.getMode() != WIFI_OFF) {
          Serial.println("WiFi wasn't off!");
          WiFi.persistent(true);
          WiFi.mode(WIFI_OFF);
        }
        WiFiUP(false);
      }
  } 
  int mins = cache.ActiveTime/60000;
  
  // check if ESP returns from deepsleep
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
        Serial.println("ESP woke up due to timer.");
  }
  
  if(!doUpdate) {
     Serial.println("No Update. Sleeping..."); 
     doSleep(); 
  }

  if (doBLE) {
    std::string bleFinal = "";
    char buff[250];
    Serial.print("Doing BLE advertisement for name: ");
    Serial.printf("LVL=%i:B=%.3f:MIN=%i\r\n", (int)cache.tankPercentage, cache.BatteryVoltage,mins);
    sprintf(buff,"LVL=%i:B=%.3f:MIN=%i", (int)cache.tankPercentage, cache.BatteryVoltage,mins);
    bleFinal += buff;
    BLEDevice::init(bleFinal);
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();
    delay(5000);
    pAdvertising->stop();
    BLEDevice::getAdvertising()->stop();
  }
  if (doWifi) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFiUP(true);
    }
    doUpdate=0;
    
    if (WiFi.status() == WL_CONNECTED) {
      // connect to webserver/hubitat.
      if (doHubitat == 1) {
        Serial.println("[HTTPClient] connect to hubitat...\n");
        String serverPath = HubitatApp + "/" + cache.BatteryVoltage + "/" + (int)cache.tankPercentage + "/" + (int)cache.NumberOfRestarts + "/" + (int)mins + "?access_token=" + HubitatAccess;
        Serial.println(serverPath);   
        http.begin(serverPath.c_str());
        int httpResponseCode = http.GET();
        if (httpResponseCode>0) {
              Serial.print("HTTP Response code: ");
              Serial.println(httpResponseCode);
              String payload = http.getString();
              Serial.println(payload);
        } else {
              Serial.print("Error code: ");
              Serial.println(httpResponseCode);
        }
        http.end();
      }
      if (doGenmon == 1 && doPropane == 1) {
        // connect to genmon. 
        Serial.println("[WiFiClient] connect to genmon...\n");
        if (!client.connect(host, port)) {
            Serial.println("Connection to host failed");
            delay(10000);
            return;
        }
      }
    }
  }
}


/******************************************************************************
Description.: reads the battery voltage through the voltage divider at GPIO34
              if the ESP32-E has calibration eFused those will be used.
              In comparison with a regular voltmeter the values of ESP32 and
              multimeter differ only about 0.05V
Input Value.: -
Return Value: battery voltage in volts
******************************************************************************/
float readBattery() {
  uint32_t value = 0;
  int rounds = 4;
  esp_adc_cal_characteristics_t adc_chars;

  //battery voltage divided by 2 can be measured at GPIO34, which equals ADC1_CHANNEL6
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  switch(esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars)) {
    case ESP_ADC_CAL_VAL_EFUSE_TP:
      Serial.println("Characterized using Two Point Value");
      break;
    case ESP_ADC_CAL_VAL_EFUSE_VREF:
      Serial.printf("Characterized using eFuse Vref (%d mV)\r\n", adc_chars.vref);
      break;
    default:
      Serial.printf("Characterized using Default Vref (%d mV)\r\n", 1100);
  }

  //to avoid noise, sample the pin several times and average the result
  for(int i=1; i<=rounds; i++) {
    value += adc1_get_raw(ADC1_CHANNEL_6);
  }
  value /= (uint32_t)rounds;

  //due to the voltage divider (1M+1M) values must be multiplied by 2
  //and convert mV to V
  return esp_adc_cal_raw_to_voltage(value, &adc_chars)*2.0/1000.0;
}

/******************************************************************************
Description.: bring the WiFi up
Input Value.: When "tryCachedValuesFirst" is true the function tries to use
              cached values before attempting a scan + association
Return Value: true if WiFi is up, false if it timed out
******************************************************************************/
bool WiFiUP(bool tryCachedValuesFirst) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  
  if(tryCachedValuesFirst && cache.channel > 0) {
    Serial.println(F("Cached values as follows:"));
    Serial.printf(" Channel....: %d\r\n", cache.channel);
    Serial.printf(" BSSID......: %x:%x:%x:%x:%x:%x\r\n", cache.bssid[0], \
                                                         cache.bssid[1], \
                                                         cache.bssid[2], \
                                                         cache.bssid[3], \
                                                         cache.bssid[4], \
                                                         cache.bssid[5]);

    WiFi.begin(ESSID, PSK, cache.channel, cache.bssid);

    for (unsigned long i=millis(); millis()-i < 10000;) {
      delay(10);

      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi connected with cached values (%lu)\r\n", millis()-i);
        return true;
      } 
    }
  }

  cache.channel = 0;
  for (uint32_t i = 0; i < sizeof(cache.bssid); i++)
    cache.bssid[i] = 0;

  // try it with the slower process
  WiFi.begin(ESSID, PSK);
  
  for (unsigned long i=millis(); millis()-i < 60000;) {
    delay(10);
  
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("WiFi connected (%lu)\r\n", millis()-i);
  
      uint8_t *bssid = WiFi.BSSID();
      for (uint32_t i = 0; i < sizeof(cache.bssid); i++)
        cache.bssid[i] = bssid[i];
      cache.channel = WiFi.channel();
    
      return true;
    }
  }

  Serial.println("WiFi NOT connected; will try again in 2 minutes");
  //doSleep();
  esp_sleep_enable_timer_wakeup(120000000); // 120 secs
  esp_deep_sleep_start();
  return false;
}

float getSample () {
  uint8_t i;
  float average;
  int samples[NUMSAMPLES];

  // take N samples in a row, with a slight delay
  for (i=0; i< NUMSAMPLES; i++) {
   samples[i] = analogRead(ANALOGPIN);
   delay(10);
  }
  
  // average all the samples out
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
     average += samples[i];
  }
  average /= NUMSAMPLES;
  return average * (SCALE) + (OFFSET) + ADD;
}

void doSleep() {
  if (doWifi) {
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.disconnect(true, true);
    }
  }
  cache.ActiveTime += millis();
  Serial.printf("=== entering deepsleep after %d ms ===\r\n", millis());
  digitalWrite(2, LOW);
  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP*uS_TO_S_FACTOR);
  //esp_sleep_enable_timer_wakeup(3600000000); // 1 hr
  //esp_sleep_enable_timer_wakeup(15000000); // 15 secs
  //esp_sleep_enable_timer_wakeup(60000000); // 1 min
  //esp_sleep_enable_timer_wakeup(180000000)// 3 mins
  esp_sleep_enable_timer_wakeup(900000000); // 15 min
  esp_deep_sleep_start();
}

void loop() {
   // while we get data from genmon client
   String inData = "";
   if (doGenmon == 1) {
     while ((client.available()) && (isSent==0)) {
      char c = client.read();
      inData += c; 
      int size = inData.length();
      if ((inData[0] == 'O') && (inData[1] == 'K') && (inData[3] == ':')) {
        // Connected OK
        Serial.print("Connected: ");
        Serial.println(inData);
        isSent=1;
        inData = "";
        break;
      } else if ((inData[size-14] == 'O') && (inData[size-13] == 'K') && (inData[size-12] == 'E')) {
        // Received command and responded: OKEndOfMessage
        Serial.println("Received OKEndOfMessage: Sleeping");
        doPropane=0;
        inData = "";
        client.stop();
        client.flush();  
        doSleep(); 
      }
    }
  
    while ((client.connected()) && (isSent==1)) {
        Serial.println("Ready to send");
        //cache.tankPercentage=69;
        String send = "generator: set_tank_data={\"Tank Name\": \""+ tankname + "\", \"Capacity\": " + capacity + ", \"Percentage\": " + (int)cache.tankPercentage + "}\r\n";
        //String send = "generator: set_tank_data={\"Tank Name\": \""+ tankname + "\", \"Capacity\": " + capacity + ", \"Percentage\": \"69\"}\r\n";
        int size = send.length()+1;
        char outData[size];
        send.toCharArray(outData, size);
        client.print(outData);
        Serial.print("Sent tank data to Genmon: ");
        Serial.println(outData);
        isSent=0;
        break;
    }
  
    if (!client.connected()) {
      Serial.println("closing connection\n");     
      client.stop();
      // do nothing:
      client.flush();   
      doSleep();
    } 
   } else {
      doSleep();
   }
}
