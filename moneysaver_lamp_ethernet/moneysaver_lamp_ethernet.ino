/***************************************************************************/
#define LEDPIN 8
#define LEDONE 8
#define LEDNUM 111
#define MAXBRIGHTNESS 17
#define BLUEBRIGHTNESS 129

const char kHostname[] = "185.20.136.207";
const char kPath[] = "/gui/api/data?sensor=Power100";

/* Data from server:
 *
 * ["watts 0.."]
 *   or
 * ["watts 0..", "power 0..255", "mood 0..255"]
 */

#define MAXSCALE 10000
#define GREENLVL 5000
#define REDLVL 8000

static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

const int kNetworkTimeout = 1000;
const int kNetworkDelay = 100;
/***************************************************************************/
#include <SPI.h>
#include <HttpClient.h>
#include <Ethernet.h>

#include <Adafruit_NeoPixel.h>

#include <MemoryFree.h>

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDONE + LEDNUM + 1, LEDPIN, NEO_GRB + NEO_KHZ800);

static uint16_t dotpixel = 0;
static uint32_t dotcolor = strip.Color(0, MAXBRIGHTNESS, 0);

unsigned long lastTick = 0;

static void setupUART() {
  Serial.begin(57600);
  Serial.println("BOOT");
}

static void setupLEDs() {
  strip.begin();
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0, ((i < LEDONE) || i >= (LEDONE + LEDNUM)) ? 0 : MAXBRIGHTNESS, 0);
  }
  ledDot(127);
  strip.show();
}

static void animLEDs() {
  ledDot(255);
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    ledGrow(255);
    ledShift();
    strip.show();
    delay(10);
  }
  ledDot(0);
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    ledGrow(0);
    ledShift();
    strip.show();
    delay(10);
  }
  ledDot(127);
  strip.show();
}

static void setupEthernet() {
  Serial.print("MAC: ");
  for (byte i = 0; i < 6; ++i) {
    Serial.print(mymac[i], HEX);
    if (i < 5)
      Serial.print(':');
  }
  Serial.println();
}

static void setupIP() {
  Serial.println("DHCP...");
  while (Ethernet.begin(mymac) != 1)
  {
    Serial.println("Error getting IP address via DHCP, trying again...");
    delay(1000);
    checkTick();
  }
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }
  Serial.println();
}

void setup() {
  setupUART();
  setupLEDs();
  animLEDs();
}

static void ledShift() {
  uint32_t color;
  for (uint16_t i = strip.numPixels() - 1; i > LEDONE; i--) {
    color = ((LEDONE + dotpixel) == (i - 1)) ? dotcolor : strip.getPixelColor(i - 1);
    if (i == (LEDONE + dotpixel))
      dotcolor = color;
    else
      strip.setPixelColor(i, color);
  }
}

static void ledGrow(byte num) {
  byte r = 0;
  byte g = 0;
  if (num < 128) {
    r = num * MAXBRIGHTNESS / 127;
    g = MAXBRIGHTNESS;
  } else {
    r = MAXBRIGHTNESS;
    g = MAXBRIGHTNESS - ((num - 128) * MAXBRIGHTNESS / 127);
  }
  uint32_t oldcol = (dotpixel == 0) ? dotcolor : strip.getPixelColor(LEDONE);
  uint32_t newcol = strip.Color(r, g, 0);
  uint32_t usecol = 0;
  byte *oci = (byte *) &oldcol;
  byte *nci = (byte *) &newcol;
  byte *uci = (byte *) &usecol;
  for (byte i = 0; i < 3; i ++)
    *uci++ = ((*nci++) + (*oci++)) / 2;
  if (dotpixel == 0) {
    dotcolor = usecol;
  } else {
    strip.setPixelColor(LEDONE, usecol);
  }
}

static void ledDot(byte num) {
  uint16_t newdot = num * (LEDNUM - 1) / 256;
  if (newdot == dotpixel)
    return;
  strip.setPixelColor(LEDONE + dotpixel, dotcolor);
  dotcolor = strip.getPixelColor(LEDONE + newdot);
  dotpixel = newdot;
  strip.setPixelColor(LEDONE + newdot, 0, 0, BLUEBRIGHTNESS);
}

static void checkTick() {
  unsigned long tick = millis();
  if ((tick - lastTick) > 1000) {
    lastTick = millis();
    ledShift();
    strip.show();
  }
}

void loop()
{
  int err =0;

  setupEthernet();
  setupIP();

  EthernetClient c;
  HttpClient http(c);

  checkTick();
  while (1) {

    Serial.println();
    Serial.print("freeMemory()=");
    Serial.println(freeMemory());

    err = http.get(kHostname, kPath);
    checkTick();
    if (err == 0) {
      Serial.println("startedRequest ok");
      err = http.responseStatusCode();
      checkTick();
      if (err >= 0) {
        Serial.print("Got status code: ");
        Serial.println(err);
        err = http.skipResponseHeaders();
        checkTick();
        if (err >= 0) {
          int bodyLen = http.contentLength();
          Serial.print("Content length is: ");
          Serial.println(bodyLen);
          Serial.println("Body returned follows:");
          checkTick();
          unsigned long timeoutStart = millis();
          char c;
          int col = 0;
          int num = 0;
          byte fdata = 0;
          while ((http.connected() || http.available()) &&
                 ((millis() - timeoutStart) < kNetworkTimeout)) {
            if (http.available()) {
              if (bodyLen <= 0)
                break;
              c = http.read();
              bodyLen--;

              if (c == '[')
                fdata = 1;

              if (! fdata)
                continue;

              while (1) {
                if (bodyLen <= 0)
                  break;
                c = http.read();
                bodyLen--;
                Serial.print(c);
                if (c == ']') {
                  if (col == 0) {
                    if (num > MAXSCALE)
                      num = MAXSCALE;
                    byte power = num / (MAXSCALE / 256);
                    if (num < GREENLVL)
                      num = GREENLVL;
                    if (num > REDLVL)
                      num = REDLVL;
                    byte mood = (num - GREENLVL) / ((REDLVL - GREENLVL) / 256);
                    ledGrow(mood);
                    ledDot(power);
                    num = 0;
                  } else if (col == 2) {
                    ledGrow(num);
                  }
                }
                if (c == ',') {
                  if (col == 1) {
                    ledDot(num);
                  }
                  num = 0;
                  col++;
                }

                if ((c == '\0') || (c == ']'))
                  break;
                if ((c >= '0') && (c <= '9')) {
                  num = num * 10;
                  num += (byte)c - (byte)'0';
                }
              }
              timeoutStart = millis();
            } else {
                delay(kNetworkDelay);
            }
          }
        } else {
          Serial.print("Failed to skip response headers: ");
          Serial.println(err);
          http.stop();
          return;
        }
      } else {
        Serial.print("Getting response failed: ");
        Serial.println(err);
      }
    } else {
      Serial.print("Connect failed: ");
      Serial.println(err);
    }
    http.stop();
  }
}
