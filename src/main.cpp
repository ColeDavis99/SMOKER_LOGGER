#include <Arduino.h>
#include <ArduinoOTA.h> //For Over-The-Air programming (OTA)
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <ESPmDNS.h>
#include "max6675.h"

/* --- WiFi Credentials --- */
const char *ssid = "";    // SSID
const char *password = ""; // Password

/* --- Server & Dashboard Setup --- */
AsyncWebServer server(80);
ESPDash dashboard(server);
dash::GenericCard countdown(dashboard, "Countdown");

/* --- Declare Dashboard Charts & Cards --- */
//T5 is the original 03/19/26
dash::SeparatorCard sep5(dashboard, "T5 Probe");
dash::TemperatureCard tempcard5(dashboard, "Live Temp T5");
dash::TemperatureCard lastAverageTempCard5(dashboard, "Latest average temp T5");
dash::BarChart<const char *, float> bar5(dashboard, "T5 Temp Log (24 hours)");

//T1 is the new kid on the block
dash::SeparatorCard sep1(dashboard, "T1 Probe");
dash::TemperatureCard tempcard1(dashboard, "Live Temp T1");
dash::TemperatureCard lastAverageTempCard1(dashboard, "Latest average temp T1");
dash::BarChart<const char *, float> bar1(dashboard, "T1 Temp Log (24 hours)");

/* --- Global Configuration --- */
const int PING_DELAY = 1000;                                                   // How quickly the webpage updates in ms
const int NUM_TEMP_SAMPLES = 60 ;                                              // "Resolution" of your average temperature in ms (how many samples to take across TOT_TEMP_SAMPLE_RANGE)
const unsigned int TOT_TEMP_SAMPLE_RANGE = 60000;                              // Each bar in the history temp chart will be the average temp across this many ms
const int SINGLE_TEMP_SAMPLE_DELAY = TOT_TEMP_SAMPLE_RANGE / NUM_TEMP_SAMPLES; // ^...equally spaced out readings that is
const unsigned long MAX_POINTS = 2;                                         // Max # of bars in chart before scrolling visual begins

/* --- Storage of Temp Readings --- */
char labels5[MAX_POINTS][12];
char labels1[MAX_POINTS][12];
const char *XAxis5[MAX_POINTS];
const char *XAxis1[MAX_POINTS];

float YAxis5[MAX_POINTS];
float YAxis1[MAX_POINTS];

/* --- Pin & Sensor Config --- */
const int thermoCS5 = 32;
const int thermoCS1 = 18;
const int thermoSO = 33;    //Shared line between all MAX6675 modules
const int thermoCLK = 25;   //Shared line between all MAX6675 modules
MAX6675 thermocouple5(thermoCLK, thermoCS5, thermoSO);
MAX6675 thermocouple1(thermoCLK, thermoCS1, thermoSO);

/* --- Non-Blocking Timing Variables --- */
int pingctr = 0;                  // For the "seconds till next average" card value
unsigned long lastPingTime = 0;   // For refreshing the dashboard
unsigned long lastSampleTime = 0; // For spacing out temp readings for the average
int sampleCtr = 0;                // The number of temperature samples the next bar in the bar chart has had done

/* --- Temperature Averaging Variables --- */
float runningT_sum5 = 0.0;    // Sum of the ongoing average's temp readings so far
float runningT_sum1 = 0.0;    // Sum of the ongoing average's temp readings so far

unsigned long updateCtr = 0; // # of average readings have been calculated (when to begin scrolling the barchart instead of appending)
float currentT5 = 0.0;        // Stores the latest calculated average
float currentT1 = 0.0;        // Stores the latest calculated average

float liveT5 = 0.0;           // Stores the live temperature
float liveT1 = 0.0;           // Stores the live temperature

void setup()
{
  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // High performance mode, fixes mDNS "beer.local" not connecting occasionally
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nIP Address: " + WiFi.localIP().toString());

  if(
    !MDNS.begin("smoker")){Serial.println("Error starting mDNS");}
  else{
    Serial.println("mDNS started: http://smoker.local");}

  server.begin();
}

void loop()
{
  unsigned long currentMillis = millis();

  // --- STEP 0: Frequent Dashboard Update For Countdown---
  if (currentMillis - lastPingTime >= PING_DELAY)
  {
    lastPingTime = millis();
    if (PING_DELAY * pingctr >= TOT_TEMP_SAMPLE_RANGE)
    {
      pingctr = 0;
    } // Reset countdown timer

    int secondsLeft = ((TOT_TEMP_SAMPLE_RANGE - (PING_DELAY * pingctr)) / 1000);
    char countBuf[12];               // Temporary buffer for this block
    itoa(secondsLeft, countBuf, 10); // Convert number to C-string
    strcat(countBuf, "s");           // Add the 's' unit
    countdown.setValue(countBuf);    // Send raw char array

    pingctr++;
    dashboard.sendUpdates(); // Render changes on webpage
  }

  // --- STEP 1: Non-Blocking Sampling ---
  if (currentMillis - lastSampleTime >= SINGLE_TEMP_SAMPLE_DELAY)
  {
    liveT5 = thermocouple5.readFahrenheit();
    liveT1 = thermocouple1.readFahrenheit();

    runningT_sum5 += liveT5;
    runningT_sum1 += liveT1;

    sampleCtr++;
    tempcard5.setValue(liveT5);
    tempcard1.setValue(liveT1);

    lastSampleTime = currentMillis;

    // --- STEP 2: Process Average & Update Dashboard ---
    if (sampleCtr >= NUM_TEMP_SAMPLES)
    {
      currentT5 = runningT_sum5 / NUM_TEMP_SAMPLES;
      currentT1 = runningT_sum1 / NUM_TEMP_SAMPLES;

      // Reset for next batch
      runningT_sum5 = 0;
      runningT_sum1 = 0;

      sampleCtr = 0;

      // Update Cards
      tempcard5.setValue(currentT5);
      tempcard1.setValue(currentT1);

      lastAverageTempCard5.setValue(currentT5);
      lastAverageTempCard1.setValue(currentT1);

      updateCtr++;
      // Update Chart (Rolling Buffer Logic)
      if (updateCtr <= MAX_POINTS)
      {
        int index = updateCtr - 1;
        ltoa(updateCtr, labels5[index], 10);
        ltoa(updateCtr, labels1[index], 10);

        XAxis5[index] = labels5[index];
        XAxis1[index] = labels1[index];

        YAxis5[index] = currentT5;
        bar5.setX(XAxis5, updateCtr);
        bar5.setY(YAxis5, updateCtr);

        YAxis1[index] = currentT1;
        bar1.setX(XAxis1, updateCtr);
        bar1.setY(YAxis1, updateCtr);
      }
      else
      {
        // Shift data left
        for (int i = 0; i < MAX_POINTS - 1; i++)
        {
          strcpy(labels5[i], labels5[i + 1]);
          strcpy(labels1[i], labels1[i + 1]);

          YAxis5[i] = YAxis5[i + 1];
          YAxis1[i] = YAxis1[i + 1];

          XAxis5[i] = labels5[i];
          XAxis1[i] = labels1[i];
        }
        // Add new data to end
        ltoa(updateCtr, labels5[MAX_POINTS - 1], 10);
        ltoa(updateCtr, labels1[MAX_POINTS - 1], 10);

        YAxis5[MAX_POINTS - 1] = currentT5;
        YAxis1[MAX_POINTS - 1] = currentT1;

        XAxis5[MAX_POINTS - 1] = labels5[MAX_POINTS - 1];
        XAxis1[MAX_POINTS - 1] = labels1[MAX_POINTS - 1];

        bar5.setX(XAxis5, MAX_POINTS);
        bar5.setY(YAxis5, MAX_POINTS);

        bar5.setX(XAxis1, MAX_POINTS);
        bar5.setY(YAxis1, MAX_POINTS);
      }

      Serial.print("Avg Temp: ");
      Serial.println(currentT5);

      Serial.print("Avg Temp: ");
      Serial.println(currentT1);
    }
  }

  // No delay() here! The ESP32 is free to handle web requests instantly.
}




/*==================================================================================================
 * Below is the MVP for reading from a single MAX6675 module to the Serial Monitor
 *==================================================================================================*/

// #include <Arduino.h>
// #include "max6675.h"

// const int thermoSO = 33;
// const int thermoCS = 32;
// const int thermoCLK = 25;

// MAX6675 thermocouple(thermoCLK, thermoCS, thermoSO);

// void setup()
// {
//   Serial.begin(9600);

//   Serial.println("MAX6675 test");
//   // wait for MAX chip to stabilize
//   delay(500);
// }

// void loop()
// {
//   // basic readout test, just print the current temp

//   // Serial.print("C = ");
//   // Serial.println(thermocouple.readCelsius());
//   Serial.print("F = ");
//   Serial.println(thermocouple.readFahrenheit());

//   // For the MAX6675 to update, you must delay AT LEAST 250ms between reads!
//   delay(1000);
// }

/*========================================================================================*/
