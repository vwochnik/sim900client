/**
 * Example showing the use of the SIM900Client library with the HttpClient
 * library.
 *
 * Install the HttpClient library from: https://github.com/amcewen/HttpClient
 */

// SIM900Client depends on SoftwareSerial library
#include <SoftwareSerial.h>
// SIM900Client library
#include <SIM900Client.h>
// HttpClient library by Adrian McEwen
#include <HttpClient.h>

// the mobile network login data
#define GPRS_APN "public4.m2minternet.com"
#define GPRS_USER ""
#define GPRS_PASS ""

// This example downloads the URL "http://arduino.cc/"

// Name of the server we want to connect to
const char kHostname[] = "arduino.cc";
// Path to download (this is the bit after the hostname in the URL
// that you want to download
const char kPath[] = "/";

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

// SIM900Client library instantiation
// if you own the GSMSHIELD
SIM900Client sim(7, 8, 9);
// or if you own the GBOARD
//SIM900Client sim(2, 3, 6);

HttpClient http(sim);

void setup()
{
  // initialize serial communications at 9600 bps:
  Serial.begin(9600);

  sim.begin(2400); while (!sim);
  if (!sim.attach(GPRS_APN, GPRS_USER, GPRS_PASS)) {
    Serial.println(F("Could not attach."));
    for(;;);
  }
}

void loop()
{
  int err =0;

  err = http.get(kHostname, kPath);
  if (err == 0)
  {
    Serial.println(F("startedRequest ok"));

    err = http.responseStatusCode();
    if (err >= 0)
    {
      Serial.print(F("Got status code: "));
      Serial.println(err);

      // Usually you'd check that the response code is 200 or a
      // similar "success" code (200-299) before carrying on,
      // but we'll print out whatever response we get

      err = http.skipResponseHeaders();
      if (err >= 0)
      {
        int bodyLen = http.contentLength();
        Serial.print(F("Content length is: "));
        Serial.println(bodyLen);
        Serial.println();
        Serial.println(F("Body returned follows:"));

        // Now we've got to the body, so we can print it out
        unsigned long timeoutStart = millis();
        char c;
        // Whilst we haven't timed out & haven't reached the end of the body
        while ( (http.connected() || http.available()) &&
               ((millis() - timeoutStart) < kNetworkTimeout) )
        {
            if (http.available())
            {
                c = http.read();
                // Print out this character
                Serial.print(c);

                bodyLen--;
                // We read something, reset the timeout counter
                timeoutStart = millis();
            }
            else
            {
                // We haven't got any data, so let's pause to allow some to
                // arrive
                delay(kNetworkDelay);
            }
        }
      }
      else
      {
        Serial.print(F("Failed to skip response headers: "));
        Serial.println(err);
      }
    }
    else
    {
      Serial.print(f("Getting response failed: "));
      Serial.println(err);
    }
  }
  else
  {
    Serial.print(F("Connect failed: "));
    Serial.println(err);
  }
  http.stop();

  // And just stop, now that we've tried a download
  while(1);
}
