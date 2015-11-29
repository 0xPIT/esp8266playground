/*
  Analog Clock Face with NTP with 1.8" TFT (ST7735) running on ESP8266
  NTP code extracted from TimeNTP_ESP8266Wifi.ino included in `Time` by Paul Stoffregen
  (c) 2015 Karl Pitrich, MIT Licensed

  Dependencies:
    * https://github.com/adafruit/Adafruit-GFX-Library.git
    * https://github.com/nzmichaelh/Adafruit-ST7735-Library.git
    * https://github.com/PaulStoffregen/Time.git
*/

#include <TimeLib.h> 
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>


#define ColorPrimary ST7735_BLACK
#define ColorBG ST7735_WHITE

const float degToRad = 0.0174532925;  // 1° == 0.0174532925rad 

const char ssid[] = "...";            // network SSID
const char pass[] = "...";  // network password

const int timeZone = 1;               // Central European Time
const char* timerServerDNSName = "time.nist.gov";
IPAddress timeServer;

WiFiUDP Udp;
const unsigned int localPort = 8888;  // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;       // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];   // buffer to hold incoming & outgoing packets

#define TFT_CS     15
#define TFT_DC     2
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC);

typedef struct Point_s {
  uint16_t x;
  uint16_t y;
} Point_t;

Point_t displayCenter = { // 160x128 with rotation=3
  ST7735_TFTHEIGHT_18 / 2,
  ST7735_TFTWIDTH / 2
};

// radius of the clock face
uint16_t clockRadius = ((displayCenter.x < displayCenter.y) ? displayCenter.x : displayCenter.y) - 2;

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }

  Serial.println("No NTP response");
  return 0;
}

void drawHourTick(uint8_t hour, Point_t center, uint16_t radius)
{
  float radians = hour * PI / 6.0;

  tft.drawLine(
    center.x + (int)((0.91 * radius * sin(radians))),
    center.y - (int)((0.91 * radius * cos(radians))),
    center.x + (int)((1.00 * radius * sin(radians))),
    center.y - (int)((1.00 * radius * cos(radians))),
    ColorPrimary
  );
}

void drawMinuteDot(uint8_t minute, Point_t center, uint16_t radius)
{
    float radians = minute * PI / 30.0;

    if (minute % 15 == 0) {
      tft.fillCircle(
        center.x + (int)((0.91 * radius * sin(radians))),
        center.y - (int)((0.91 * radius * cos(radians))),
        2,
        ColorPrimary
      );
    }
    else {
      tft.drawPixel(
        center.x + (int)((0.91 * radius * sin(radians))),
        center.y - (int)((0.91 * radius * cos(radians))),
        ColorPrimary
      );
    }
}

void drawClockFace(uint16_t radius, Point_t center) {
  tft.fillScreen(ColorBG);
  tft.setRotation(3);

  for (uint8_t hr = 1; hr <= 12; hr++) {
    drawHourTick(hr, center, radius);
  }

  for (uint8_t min = 1; min <= 60; min++) {
    drawMinuteDot(min, center, radius);
  }
}

void drawClockHands(time_t now, uint16_t radius, Point_t center) {
  float radians;
  uint8_t hh = hour(now);
  uint8_t mm = minute(now);
  uint8_t ss = second(now);

  uint16_t color = ColorPrimary;
  tft.fillCircle(center.x, center.y, radius * 0.85, ColorBG);

  // hour
  radians = (hh % 12) * PI / 6.0 + (PI * mm / 360.0);
  tft.drawLine(
    center.x, center.y,
    center.x + (int)(radius * 0.5 * sin(radians)),
    center.y - (int)(radius * 0.5 * cos(radians)),
    color 
  );

  // hinute
  radians = (mm * PI / 30.0) + (PI * ss / 1800.0);
  tft.drawLine(
    center.x, center.y,
    center.x + (int)(0.7 * radius * sin(radians)),
    center.y - (int)(0.7 * radius * cos(radians)),
    color
  );

  // second
  radians = ss * PI / 30.0;    
  tft.drawLine(
    center.x, center.y,
    center.x + (0.8 * radius * sin(radians)),
    center.y - (0.8 * radius * cos(radians)),
    ST7735_RED
  );

  // center dot
  tft.fillCircle(center.x, center.y, 3, ST7735_RED);
}

void setup() 
{
  Serial.begin(115200); 
  while (!Serial);

  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ColorBG);
  drawClockFace(clockRadius, displayCenter);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Resolving NTP Server IP ");
  WiFi.hostByName(timerServerDNSName, timeServer);
  Serial.println(timeServer.toString());

  Serial.print("Starting UDP... ");
  Udp.begin(localPort);
  Serial.print("local port: ");
  Serial.println(Udp.localPort());

  Serial.println("Waiting for NTP sync");
  setSyncProvider(getNtpTime);
}

void printDigits(Print *p, int digits) {
  p->print(":");
  if(digits < 10) {
    p->print('0');
  }
  p->print(digits);
}

void dumpClock(Print *p) {
  p->print(hour());
  printDigits(p, minute());
  printDigits(p, second());
  p->print(" ");
  p->print(day());
  p->print(".");
  p->print(month());
  p->print(".");
  p->print(year()); 
  p->println(); 
}

time_t prevTime = 0;

void loop()
{
  if (timeStatus() != timeNotSet) {
    time_t current = now();
    if (current != prevTime) {
      drawClockHands(current, clockRadius, displayCenter);
      prevTime = current;
      dumpClock(&Serial);
    }
  }
}

