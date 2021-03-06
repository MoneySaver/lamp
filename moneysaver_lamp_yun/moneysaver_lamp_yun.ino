/***************************************************************************/
#define LEDPIN 8
#define LEDONE 0
#define LEDNUM 10
#define MAXBRIGHTNESS 60

#define URL "http://sensey.ee/gui/api/data"
/***************************************************************************/
#include <Bridge.h>
#include <HttpClient.h>

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

void setup() {
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Bridge.begin();
  setupLEDs();
  setupUART();
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
  strip.setPixelColor(LEDONE + newdot, 0, 0, 255);
  strip.setPixelColor(LEDONE + newdot, 0, 0, MAXBRIGHTNESS);
  Serial.print("dotpixel=");
  Serial.println(dotpixel);
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
  HttpClient client;
  client.get(URL);

  checkTick();

  int col = 0;
  byte num = 0;

  Serial.println();
  Serial.print("freeMemory()=");
  Serial.println(freeMemory());

  while (client.available()) {
    char c = client.read();
    if (c != '[') {
      Serial.print("DATA FORMAT ERROR (");
      Serial.print(c);
      Serial.println(")");
      return;
    }

    while (client.available()) {
      char c = client.read();
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
    }
  }
}
