#include <SPI.h>
#include <HttpClient.h>
#include <Ethernet.h>

#include <Adafruit_NeoPixel.h>

#define PIN 6
const char kHostname[] = "172.19.1.165";
const char kPath[] = "/lamp.php";

Adafruit_NeoPixel strip = Adafruit_NeoPixel(100, PIN, NEO_GRB + NEO_KHZ800);

static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

const int kNetworkTimeout = 1000;
const int kNetworkDelay = 100;

static void setupUART() {
  Serial.begin(57600);
  Serial.println("BOOT");
}

static void setupLEDs() {
  strip.begin();
  for (uint16_t i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0);
  }
  strip.setBrightness(50);
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
    delay(3000);
  }
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }
  Serial.println();
}

void setup() {
  delay(3000);
  setupUART();
  setupEthernet();
  setupIP();
  setupLEDs();
}

void loop()
{
  int err =0;

  EthernetClient c;
  HttpClient http(c);

  while (1) {
    int pixel = 0;
    int col = 0;
    byte num = 0;
    byte rgb[3];

    err = http.get(kHostname, kPath);
    if (err == 0) {
      Serial.println("startedRequest ok");
      err = http.responseStatusCode();
      if (err >= 0) {
        Serial.print("Got status code: ");
        Serial.println(err);
        err = http.skipResponseHeaders();
        if (err >= 0) {
          int bodyLen = http.contentLength();
          Serial.print("Content length is: ");
          Serial.println(bodyLen);
          Serial.println("Body returned follows:");
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
                      rgb[col] = num;
                      num = 0;
                      col++;
                      if (col == 3) {
                        strip.setPixelColor(pixel, rgb[0], rgb[1], rgb[2]);
                        pixel++;
                        col = 0;
                      }
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
                  strip.show();
                  timeoutStart = millis();
              } else {
                  delay(kNetworkDelay);
              }
           }
        } else {
          Serial.print("Failed to skip response headers: ");
          Serial.println(err);
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
