#include <ESP8266WiFi.h>
#include <EEPROM.h>

#define DC_GAIN (8.5) //define the DC gain of amplifier
#define D3 0
#define D4 2
#define D7 13
#define D8 15
#define D5 14
#define D6 12

//These two values differ from sensor to sensor. user should derermine this value.
//#define ZERO_POINT_VOLTAGE (3.09 / DC_GAIN) //define the output of the sensor in volts when the concentration of CO2 is 400PPM
//#define DELTA_VOLTAGE ((ZERO_POINT_VOLTAGE - 2.79/DC_GAIN) ) //define the voltage drop of the sensor when move the sensor from air into 1000ppm CO2

unsigned long lastConnectToServer = 0;
float lastPpm = 400;
bool wifiIsSleeping = false;

const int connectToServerInterval = 2 * 60 * 1000;
const int connectTryThreshold = 12 * 1000;

class UnitData {
public:
  char SSID[10];
  char pass[20];
  float volt400;
  float volt1000;
  byte target1;
  byte target2;
  byte target3;
  byte target4;
  int port;
};

UnitData systemData;

bool USE_INFRA = false;
bool INFRA_WITH_CORRECTION = true;
unsigned long LOOP_DELAY = 30000;

bool USE_UART = true;

byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
char response[9];

#include <SoftwareSerial.h>

const int RX_PIN = D5;
const int TX_PIN = D6;
const int BAUD_RATE = 9600;

SoftwareSerial sensorUart(RX_PIN, TX_PIN);

void setup()
{
  Serial.begin(115200);

  Serial.print("Chip id ");
  Serial.println(ESP.getChipId());
 
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.setSleepMode(WIFI_MODEM_SLEEP);
  WiFi.forceSleepBegin();


  EEPROM.begin(sizeof (UnitData));
  EEPROM.get(0, systemData);
  Serial.print("Read ");
  Serial.print(systemData.volt400);
  Serial.print(' ');
  Serial.println(systemData.volt1000);
  EEPROM.end();

  // Adapt parameters if needed
  //   Formulas for function plotter 10^((x/8.5-3.35/8.5)*(2.602-3)/((3.35-3.13)/8.5)+2.602)
  //    10^((x/8.5-3.12/8.5)*(2.602-3)/((3.12-2.91)/8.5)+2.602)
  /*
  EEPROM.begin(sizeof (UnitData));
  systemData.volt400 = 3.12;
  systemData.volt1000 = 2.91;
  EEPROM.put(0, systemData);
  EEPROM.end();
  //*/

  if (USE_UART) {
    sensorUart.begin(BAUD_RATE);
  }
}

void loop()
{
  Serial.print( "all " );
  float volts1 = analogRead(A0) / 1023.0 * 3.3;
  delay(25);
  float volts2 = analogRead(A0) / 1023.0 * 3.3;
  delay(25);
  float volts3 = analogRead(A0) / 1023.0 * 3.3;
  float voltsRaw = (volts1 + volts2 + volts3) / 3.0;
  float volts = voltsRaw;
  Serial.print(volts); 
  Serial.print("V (C");
  Serial.print(volts2);
  Serial.print("V) ");

  float ppm = -1;
  if (USE_UART) {
    sensorUart.write(cmd, 9);
    sensorUart.readBytes(response, 9);

    if (0xff == response[0] && 0x86 == response[1]) {
      int responseHigh = (int) response[2];
      int responseLow = (int) response[3];
      ppm = (256 * responseHigh) + responseLow;
    } else {
      ppm = 100;
    }
  } else if (USE_INFRA) {
    ppm = volts * 1000;

    if (INFRA_WITH_CORRECTION) {
      ppm = ppm * (1 + ppm / 1250);
    }
  } else {
    volts = volts / DC_GAIN;
    Serial.print(volts, 3); 
    Serial.print("V ");
  
    float delta = systemData.volt400 / DC_GAIN - systemData.volt1000 / DC_GAIN;
    float logHighReference = 3; // 3 for 1000 or 2.903 for 800
    float logCo2 = (volts - systemData.volt400 / DC_GAIN) * (2.602 - logHighReference) / delta + 2.602;
    ppm = pow(10, logCo2);
  }

  Serial.print("CO2: ");
  Serial.print(ppm);
  Serial.print("ppm ");
  
  Serial.print("Time point: ");
  Serial.print(millis() / 60000.0);
  Serial.print("m ");
  Serial.println();

  unsigned long now = millis();
  if (now - lastConnectToServer >= connectToServerInterval) {
    // do not accumulate delay:
    lastConnectToServer = (now / connectToServerInterval) * connectToServerInterval;

    WiFi.forceSleepWake();

    WiFi.begin(systemData.SSID, systemData.pass);

    unsigned long connectStart = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - connectStart > connectTryThreshold) {
        Serial.println("Wifi connect failed (timeout)");

        /* Only needed if there is no connection (wrong environment)
        char transBuffer[30];
        sprintf(transBuffer, "PPM %d %d %dV\n", (int)((ppm + lastPpm) / 2), ESP.getChipId(), (int)(voltsRaw * 100 + 0.5));

        Serial.print(transBuffer);
        */
        
        break;
      }
      delay(10);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("ConnecteD ");
      Serial.print(WiFi.localIP());
      Serial.print(" ");
      Serial.println(millis() - connectStart);

      WiFiClient client;
      if (!client.connect(IPAddress(systemData.target1, systemData.target2, systemData.target3, systemData.target4), systemData.port)) {
        Serial.print("Server connect failed.. ");
      } else {
        Serial.println("Server connected ");
        char transBuffer[30];
        sprintf(transBuffer, "PPM %d %d %dV\n", (int)((ppm + lastPpm) / 2), ESP.getChipId(), (int)(voltsRaw * 100 + 0.5));

        Serial.print(transBuffer);
        
        client.print(transBuffer);
      }
    }

    // This has 15mA instead of 70mA (above) or 1mA with deep sleep
    WiFi.disconnect(true);
    WiFi.forceSleepBegin();

    lastPpm = ppm;
  }
  
  delay(LOOP_DELAY);
}
