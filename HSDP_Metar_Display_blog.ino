/*
   ESP32 code for METAR Display in HDSP displays.
   It uses:
      - WiFi.h for handling the conection to the wireless network.
      - HTTPClient.h for handlig the REST requests to the METAR data provider service
      - ArduinoJson.h for parsing the JSON string the METAR service returns and conver it into an string that can be sent to the displays.
      - Wire.h and MCP23008.h for managing the i2c por expanders.

      By Haritz López (lopezharitz@gmail.com)  06/12/2020
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <String.h>
#include <Wire.h>
#include "MCP23008.h"

// Two MCP23008 instances
MCP23008 addr_lines;
MCP23008 data_lines;

// WiFi connection parameters
const char* ssid     = "[YOUR SSID]";
const char* password = "[YOUR PASSWORD]";

// Control signals' pins
int RST   = 27;
int WR    = 26;
int CE1   = 12;
int CE2   = 14;
int RD    = 13;

// Some useful definitions
int AD0   = 0;
int AD1   = 1;
int AD2   = 2;
int AD3   = 3;
int AD4   = 4;
int D0    = 0;
int D1    = 1;
int D2    = 2;
int D3    = 3;
int D4    = 4;
int D5    = 5;
int D6    = 6;
int D7    = 7;


// Internal status variables
int incomingByte = 0;
char *metar;  // Metar string buffer
int metar_size = 480; // Maximum metar buffer size

/* 
 * This function is used to reset the displays. It toggles the RST pin and then pulls-up AD3 and AD4 lines
 * which are not used for basic functions as writting to character RAM.
 */
void resetDisplay()
{
  digitalWrite(RST, LOW);
  delayMicroseconds(1);
  digitalWrite(RST, HIGH);
  delayMicroseconds(150);
  addr_lines.write(AD3, HIGH);
  delayMicroseconds(1);
  addr_lines.write(AD4, HIGH);
}

/*
 * This function writes the input string into the displays. It does the writting in two steps, first the first display
 * and later the secondone. This is not optimal, but is done like this to get a wave-like effect in the update of the displays.
 */
void writeDisplay(char *input)
{
  for (int i = 0; i < 8; i++)
  {
    // set address for the characters of the first display
    addr_lines.write(AD0, (1 & i) != 0 ? HIGH : LOW);
    addr_lines.write(AD1, (2 & i) != 0 ? HIGH : LOW);
    addr_lines.write(AD2, (4 & i) != 0 ? HIGH : LOW);

    // Set datalines
    data_lines.write(D0, bitRead(input[i], 0));
    data_lines.write(D1, bitRead(input[i], 1));
    data_lines.write(D2, bitRead(input[i], 2));
    data_lines.write(D3, bitRead(input[i], 3));
    data_lines.write(D4, bitRead(input[i], 4));
    data_lines.write(D5, bitRead(input[i], 5));
    data_lines.write(D6, bitRead(input[i], 6));
    data_lines.write(D7, bitRead(input[i], 7));

    // Write to display
    delay(1);
    digitalWrite(CE1, LOW);
    delay(1);
    digitalWrite(WR, LOW);
    delay(1);
    digitalWrite(WR, HIGH);
    delay(1);
    digitalWrite(CE1, HIGH);
    delay(1);
  }

  // Now for the second display
  for (int j = 0; j < 8; j++)
  {
    // Set the address for the characters of the second display
    addr_lines.write(AD0, (1 & j) != 0 ? HIGH : LOW);
    addr_lines.write(AD1, (2 & j) != 0 ? HIGH : LOW);
    addr_lines.write(AD2, (4 & j) != 0 ? HIGH : LOW);

    // Set datalines
    data_lines.write(D0, bitRead(input[j + 8], 0));
    data_lines.write(D1, bitRead(input[j + 8], 1));
    data_lines.write(D2, bitRead(input[j + 8], 2));
    data_lines.write(D3, bitRead(input[j + 8], 3));
    data_lines.write(D4, bitRead(input[j + 8], 4));
    data_lines.write(D5, bitRead(input[j + 8], 5));
    data_lines.write(D6, bitRead(input[j + 8], 6));
    data_lines.write(D7, bitRead(input[j + 8], 7));

    // Write to the display
    delay(1);
    digitalWrite(CE2, LOW);
    delay(1);
    digitalWrite(WR, LOW);
    delay(1);
    digitalWrite(WR, HIGH);
    delay(1);
    digitalWrite(CE2, HIGH);
    delay(1);
  }
}

/* 
 * This funtion scrolls an string in both displays. In our case the METAR string.
 * It blanks the characters that tail the message until the last character has disappeared. 
 */
void scrollDisplay(char *words)
{
  char buffer[17];  // We have 16 characters to be displayed
  int i = 0;
  while (words[i] != 0)
  {
    boolean blank = false;
    for (int j = 0; j < 17; j++) // Iterate through the characters of the string and displau them
    {
      if ( !blank && words[i + j] == 0 ) // If we reached the end of the string, need to show blanc characters
      {                                  // Until the last character has disapeared on the left of the displays
        blank = true;
      }
      if ( blank )
      {
        buffer[j] = ' '; // If we are to show a blank , we send an space
      }
      else
      {
        buffer[j] = words[i + j]; // Else we show the character that corresponds
      }
    }
    buffer[16] = 0;
    writeDisplay(buffer); // Write the buffer into the display.
    delay(200); // This value can be changed to adjust scroll speed.
    i++;
  }
}

/* 
 *  This function connects the ESP32 board to the wireless network.
 */
void wifi_connect() {
  WiFi.begin(ssid, password);
  pinMode(2, OUTPUT); // We set the blue LED in the board when the connection has succeded.
  digitalWrite(2, HIGH);
}

/* 
 *  This function calls the AVWX API and gets the METAR code for the airport provided as an input.
 *  It outputs the "sanitized" METAR code or an error string
 */
char *get_metar_json(String icao_code) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String request = String("https://avwx.rest/api/metar/" + icao_code);
    http.begin(request); //Specify destination for HTTP request
    http.addHeader("Authorization",  "Token [PUT HERE YOUR TOKEN]"); // AVWX API token must be insterted in the http header
    int http_ret = http.GET();  // Perform the GET request to the service
    if (http_ret == 200) {
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, http.getString());  // Deserialice the JSON string
      if (error) {
        return ("Error Retreiving METAR");
      } else {
        auto metar = doc["sanitized"].as<const char*>();  // Get the "Sanitized" attribute, which contains the information in the format we need
        return const_cast<char*>(metar);
      }
    }
  }
  return ("Error Retreiving METAR");
}

/* 
 * Main setup function 
 */
void setup() {
  data_lines.begin(0x20); // Setup of the i2c addresses of MCP23008 objects
  addr_lines.begin(0x21);

  // All signals are going to be Outputs.
  pinMode(RST    , OUTPUT);
  pinMode(WR     , OUTPUT);
  pinMode(CE1    , OUTPUT);
  pinMode(CE2    , OUTPUT);
  pinMode(RD     , OUTPUT);
  addr_lines.pinMode(AD0    , OUTPUT);
  addr_lines.pinMode(AD1    , OUTPUT);
  addr_lines.pinMode(AD2    , OUTPUT);
  addr_lines.pinMode(AD3    , OUTPUT);
  addr_lines.pinMode(AD4    , OUTPUT);
  data_lines.pinMode(D0     , OUTPUT);
  data_lines.pinMode(D1     , OUTPUT);
  data_lines.pinMode(D2     , OUTPUT);
  data_lines.pinMode(D3     , OUTPUT);
  data_lines.pinMode(D4     , OUTPUT);
  data_lines.pinMode(D5     , OUTPUT);
  data_lines.pinMode(D6     , OUTPUT);
  data_lines.pinMode(D7     , OUTPUT);

  // Initially both displays are disabled
  digitalWrite(CE1, HIGH);
  digitalWrite(CE2, HIGH);
  digitalWrite(WR, HIGH);

  // Reset the display as characters from previous accesses can still be visible.
  resetDisplay();
  delay(350);

  // Connect to the WiFi network
  wifi_connect();
}

/* 
 * Main loop function. Gets a metar, formats it and scrolls it in the displays.
 */
void loop() {
  metar = (char*)calloc(metar_size, sizeof(char)); // This is done like this for the strcat that come below
  strcpy(metar, "                "); // Display blank on begining
  strcat(metar, get_metar_json("LEVS")); // Get the METAR for Madrid Cuatro Vientos (ICAO code: LEVS)
  strcat(metar, " "); // Clear last character
  scrollDisplay(metar);
  metar[0] = '\0';
  free(metar);
  delay(500);
}
