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
// dash::SeparatorCard sep5(dashboard, "T5 Probe");
// dash::TemperatureCard tempcard5(dashboard, "Live Temp T5");
// dash::TemperatureCard lastAverageTempCard5(dashboard, "Latest average temp T5");
// dash::BarChart<const char *, float> bar5(dashboard, "T5 Temp Log (3 hours)");

// dash::SeparatorCard sep2(dashboard, "T2 Probe");
// dash::TemperatureCard tempcard2(dashboard, "Live Temp T2");
// dash::TemperatureCard lastAverageTempCard2(dashboard, "Latest average temp T2");
// dash::BarChart<const char *, float> bar2(dashboard, "T2 Temp Log (3 hours)");

// dash::SeparatorCard sep1(dashboard, "T1 Probe");
// dash::TemperatureCard tempcard1(dashboard, "Live Temp T1");
// dash::TemperatureCard lastAverageTempCard1(dashboard, "Latest average temp T1");
// dash::BarChart<const char *, float> bar1(dashboard, "T1 Temp Log (3 hours)");

dash::SeparatorCard sep5(dashboard, "Probes");
dash::TemperatureCard tempcard5(dashboard, "Live Temp T5");
dash::TemperatureCard tempcard4(dashboard, "Live Temp T4");
dash::TemperatureCard tempcard3(dashboard, "Live Temp T3");
dash::TemperatureCard tempcard2(dashboard, "Live Temp T2");
dash::TemperatureCard tempcard1(dashboard, "Live Temp T1");

dash::SeparatorCard sep1(dashboard, "Last Minute Average");
dash::TemperatureCard lastAverageTempCard5(dashboard, "LMA T5");
dash::TemperatureCard lastAverageTempCard4(dashboard, "LMA T4");
dash::TemperatureCard lastAverageTempCard3(dashboard, "LMA T3");
dash::TemperatureCard lastAverageTempCard2(dashboard, "LMA T2");
dash::TemperatureCard lastAverageTempCard1(dashboard, "LMA T1");

dash::SeparatorCard sep2(dashboard, "Temp Logs");
dash::BarChart<const char *, float> bar5(dashboard, "T5 Temp Log (3 hours)");
dash::BarChart<const char *, float> bar4(dashboard, "T4 Temp Log (3 hours)");
dash::BarChart<const char *, float> bar3(dashboard, "T3 Temp Log (3 hours)");
dash::BarChart<const char *, float> bar2(dashboard, "T2 Temp Log (3 hours)");
dash::BarChart<const char *, float> bar1(dashboard, "T1 Temp Log (3 hours)");


/* --- Global Configuration --- */
const int PING_DELAY = 1000;                                                   // How quickly the webpage updates in ms
const int NUM_TEMP_SAMPLES = 15 ;                                              // "Resolution" of your average temperature in ms (how many samples to take across TOT_TEMP_SAMPLE_RANGE)
const unsigned int TOT_TEMP_SAMPLE_RANGE = 60000;                              // Each bar in the history temp chart will be the average temp across this many ms
const int SINGLE_TEMP_SAMPLE_DELAY = TOT_TEMP_SAMPLE_RANGE / NUM_TEMP_SAMPLES; // ^...equally spaced out readings that is
const unsigned long MAX_POINTS = 180;                                         // Max # of bars in chart before scrolling visual begins

/* --- Storage of Temp Readings --- */
char labels5[MAX_POINTS][12];
char labels4[MAX_POINTS][12];
char labels3[MAX_POINTS][12];
char labels2[MAX_POINTS][12];
char labels1[MAX_POINTS][12];

const char *XAxis5[MAX_POINTS];
const char *XAxis4[MAX_POINTS];
const char *XAxis3[MAX_POINTS];
const char *XAxis2[MAX_POINTS];
const char *XAxis1[MAX_POINTS];

float YAxis5[MAX_POINTS];
float YAxis4[MAX_POINTS];
float YAxis3[MAX_POINTS];
float YAxis2[MAX_POINTS];
float YAxis1[MAX_POINTS];

/* --- Pin & Sensor Config --- */
const int thermoCS5 = 32;
const int thermoCS4 = 17;
const int thermoCS3 = 5;
const int thermoCS2 = 26;
const int thermoCS1 = 18;
const int thermoSO = 33;    //Shared line between all MAX6675 modules
const int thermoCLK = 25;   //Shared line between all MAX6675 modules
MAX6675 thermocouple5(thermoCLK, thermoCS5, thermoSO);
MAX6675 thermocouple4(thermoCLK, thermoCS4, thermoSO);
MAX6675 thermocouple3(thermoCLK, thermoCS3, thermoSO);
MAX6675 thermocouple2(thermoCLK, thermoCS2, thermoSO);
MAX6675 thermocouple1(thermoCLK, thermoCS1, thermoSO);

/* --- Non-Blocking Timing Variables --- */
int pingctr = 0;                  // For the "seconds till next average" card value
unsigned long lastPingTime = 0;   // For refreshing the dashboard
unsigned long lastSampleTime = 0; // For spacing out temp readings for the average
int sampleCtr = 0;                // The number of temperature samples the next bar in the bar chart has had done

/* --- Temperature Averaging Variables --- */
float runningT_sum5 = 0.0;    // Sum of the ongoing average's temp readings so far
float runningT_sum4 = 0.0;    // Sum of the ongoing average's temp readings so far
float runningT_sum3 = 0.0;    // Sum of the ongoing average's temp readings so far
float runningT_sum2 = 0.0;    // Sum of the ongoing average's temp readings so far
float runningT_sum1 = 0.0;    // Sum of the ongoing average's temp readings so far

unsigned long updateCtr = 0; // # of average readings have been calculated (when to begin scrolling the barchart instead of appending)
float currentT5 = 0.0;        // Stores the latest calculated average
float currentT4 = 0.0;        // Stores the latest calculated average
float currentT3 = 0.0;        // Stores the latest calculated average
float currentT2 = 0.0;        // Stores the latest calculated average
float currentT1 = 0.0;        // Stores the latest calculated average

float liveT5 = 0.0;           // Stores the live temperature
float liveT4 = 0.0;           // Stores the live temperature
float liveT3 = 0.0;           // Stores the live temperature
float liveT2 = 0.0;           // Stores the live temperature
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
    liveT4 = thermocouple4.readFahrenheit();
    liveT3 = thermocouple3.readFahrenheit();
    liveT2 = thermocouple2.readFahrenheit();
    liveT1 = thermocouple1.readFahrenheit();

    runningT_sum5 += liveT5;
    runningT_sum4 += liveT4;
    runningT_sum3 += liveT3;
    runningT_sum2 += liveT2;
    runningT_sum1 += liveT1;

    sampleCtr++;
    tempcard5.setValue(liveT5);
    tempcard4.setValue(liveT4);
    tempcard3.setValue(liveT3);
    tempcard2.setValue(liveT2);
    tempcard1.setValue(liveT1);

    lastSampleTime = currentMillis;

    // --- STEP 2: Process Average & Update Dashboard ---
    if (sampleCtr >= NUM_TEMP_SAMPLES)
    {
      currentT5 = runningT_sum5 / NUM_TEMP_SAMPLES;
      currentT4 = runningT_sum4 / NUM_TEMP_SAMPLES;
      currentT3 = runningT_sum3 / NUM_TEMP_SAMPLES;
      currentT2 = runningT_sum2 / NUM_TEMP_SAMPLES;
      currentT1 = runningT_sum1 / NUM_TEMP_SAMPLES;

      // Reset for next batch
      runningT_sum5 = 0;
      runningT_sum4 = 0;
      runningT_sum3 = 0;
      runningT_sum2 = 0;
      runningT_sum1 = 0;

      sampleCtr = 0;

      // Update Cards
      tempcard5.setValue(currentT5);
      tempcard4.setValue(currentT4);
      tempcard3.setValue(currentT3);
      tempcard2.setValue(currentT2);
      tempcard1.setValue(currentT1);

      lastAverageTempCard5.setValue(currentT5);
      lastAverageTempCard4.setValue(currentT4);
      lastAverageTempCard3.setValue(currentT3);
      lastAverageTempCard2.setValue(currentT2);
      lastAverageTempCard1.setValue(currentT1);

      updateCtr++;
      // Update Chart (Rolling Buffer Logic)
      if (updateCtr <= MAX_POINTS)
      {
        int index = updateCtr - 1;
        ltoa(updateCtr, labels5[index], 10);
        ltoa(updateCtr, labels4[index], 10);
        ltoa(updateCtr, labels3[index], 10);
        ltoa(updateCtr, labels2[index], 10);
        ltoa(updateCtr, labels1[index], 10);

        XAxis5[index] = labels5[index];
        XAxis4[index] = labels4[index];
        XAxis3[index] = labels3[index];
        XAxis2[index] = labels2[index];
        XAxis1[index] = labels1[index];

        YAxis5[index] = currentT5;
        YAxis4[index] = currentT4;
        YAxis3[index] = currentT3;
        YAxis2[index] = currentT2;
        YAxis1[index] = currentT1;

        bar5.setX(XAxis5, updateCtr);
        bar4.setX(XAxis4, updateCtr);
        bar3.setX(XAxis3, updateCtr);
        bar2.setX(XAxis2, updateCtr);
        bar1.setX(XAxis1, updateCtr);
        
        bar5.setY(YAxis5, updateCtr);
        bar4.setY(YAxis4, updateCtr);
        bar3.setY(YAxis3, updateCtr);
        bar2.setY(YAxis2, updateCtr);
        bar1.setY(YAxis1, updateCtr);
      }
      else
      {
        // Shift data left
        for (int i = 0; i < MAX_POINTS - 1; i++)
        {
          strcpy(labels5[i], labels5[i + 1]);
          strcpy(labels4[i], labels4[i + 1]);
          strcpy(labels3[i], labels3[i + 1]);
          strcpy(labels2[i], labels2[i + 1]);
          strcpy(labels1[i], labels1[i + 1]);

          YAxis5[i] = YAxis5[i + 1];
          YAxis4[i] = YAxis4[i + 1];
          YAxis3[i] = YAxis3[i + 1];
          YAxis2[i] = YAxis2[i + 1];
          YAxis1[i] = YAxis1[i + 1];

          XAxis5[i] = labels5[i];
          XAxis4[i] = labels4[i];
          XAxis3[i] = labels3[i];
          XAxis2[i] = labels2[i];
          XAxis1[i] = labels1[i];
        }
        // Add new data to end
        ltoa(updateCtr, labels5[MAX_POINTS - 1], 10);
        ltoa(updateCtr, labels4[MAX_POINTS - 1], 10);
        ltoa(updateCtr, labels3[MAX_POINTS - 1], 10);
        ltoa(updateCtr, labels2[MAX_POINTS - 1], 10);
        ltoa(updateCtr, labels1[MAX_POINTS - 1], 10);

        YAxis5[MAX_POINTS - 1] = currentT5;
        YAxis4[MAX_POINTS - 1] = currentT4;
        YAxis3[MAX_POINTS - 1] = currentT3;
        YAxis2[MAX_POINTS - 1] = currentT2;
        YAxis1[MAX_POINTS - 1] = currentT1;

        XAxis5[MAX_POINTS - 1] = labels5[MAX_POINTS - 1];
        XAxis4[MAX_POINTS - 1] = labels4[MAX_POINTS - 1];
        XAxis3[MAX_POINTS - 1] = labels3[MAX_POINTS - 1];
        XAxis2[MAX_POINTS - 1] = labels2[MAX_POINTS - 1];
        XAxis1[MAX_POINTS - 1] = labels1[MAX_POINTS - 1];

        bar5.setX(XAxis5, MAX_POINTS);
        bar4.setX(XAxis4, MAX_POINTS);
        bar3.setX(XAxis3, MAX_POINTS);
        bar2.setX(XAxis2, MAX_POINTS);
        bar1.setX(XAxis1, MAX_POINTS);
        
        bar5.setY(YAxis5, MAX_POINTS);
        bar4.setY(YAxis4, MAX_POINTS);
        bar3.setY(YAxis3, MAX_POINTS);
        bar2.setY(YAxis2, MAX_POINTS);
        bar1.setY(YAxis1, MAX_POINTS);
      }

      // Serial.print("Avg Temp: ");
      // Serial.println(currentT5);

      // Serial.print("Avg Temp: ");
      // Serial.println(currentT2);

      // Serial.print("Avg Temp: ");
      // Serial.println(currentT1);
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
