/***************************************************************************/
#define LEDPIN 6
#define LEDNUM 120
#define MAXBRIGHTNESS 80

const char kHostname[] = "185.20.136.207";
const char kPath[] = "/gui/api/data";

static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

const int kNetworkTimeout = 1000;
const int kNetworkDelay = 100;
/***************************************************************************/
#include <SPI.h>
#include <HttpClient.h>
#include <Ethernet.h>

#include <Adafruit_NeoPixel.h>

#include <MemoryFree.h>

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDNUM, LEDPIN, NEO_GRB + NEO_KHZ800);

static uint16_t dotpixel = 0 ;
static uint32_t dotcolor = strip.Color(0, MAXBRIGHTNESS, 0);

unsigned long lastTick = millis();

static void setupUART() {
  Serial.begin(57600);
  Serial.println("BOOT");
}

static void setupLEDs() {
  strip.begin();
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0, MAXBRIGHTNESS, 0);
  }
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
  setupEthernet();
  setupIP();
  setupLEDs();
}

static void ledShift() {
  uint32_t color;
  for (uint16_t i = strip.numPixels() - 1; i > 0; i--) {
    color = (dotpixel == (i - 1)) ? dotcolor : strip.getPixelColor(i - 1);
    if (dotpixel == i)
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
  uint32_t oldcol = strip.getPixelColor(1);
  uint32_t newcol = strip.Color(r, g, 0);
  uint32_t usecol = 0;
  byte *oci = (byte *) &oldcol;
  byte *nci = (byte *) &newcol;
  byte *uci = (byte *) &usecol;
  for (byte i = 0; i < 3; i ++)
    *uci++ = ((*nci++) + (*oci++)) / 2;
  strip.setPixelColor(0, usecol);
}

static void ledDot(byte num) {
  uint16_t newdot = num * (LEDNUM - 1) / 256;
  strip.setPixelColor(dotpixel, dotcolor);
  dotpixel = newdot;
  dotcolor = strip.getPixelColor(newdot);
  strip.setPixelColor(newdot, 0, 0, 255);
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

  EthernetClient c;
  HttpClient http(c);

  checkTick();
  while (1) {
    int col = 0;
    byte num = 0;

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
          while ((http.connected() || http.available()) &&
                 ((millis() - timeoutStart) < kNetworkTimeout)) {
            if (http.available()) {
              c = http.read();
              if (c != '[') {
                Serial.print("DATA FORMAT ERROR (");
                Serial.print(c);
                Serial.println(")");
                http.stop();
                return;
              }

              while (1) {
                c = http.read();
                Serial.print(c);
                if ((c == ',') || (c == ']')) {
                  if (col == 0) {
                    ledGrow(num);
                  } else if (col == 1) {
                    ledDot(num);
                  }
                  col++;
                  num = 0;
                }
                if ((c == '\0') || (c == ']'))
                  break;
                if ((c >= '0') && (c <= '9')) {
                  num = num * 10;
                  num += (byte)c - (byte)'0';
                }
                bodyLen--;
                if (bodyLen == 0)
                  break;
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
