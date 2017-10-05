#include <ESP8266WiFi.h>
#include <EEPROM.h>

#define D2 4
#define DC_GAIN (8.5) //define the DC gain of amplifier

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

void setup()
{
  Serial.begin(115200);
  pinMode(D2, INPUT_PULLUP);

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
  //   Formula for function plotter (10^((x/8.5-3.4/8.5)*(2.602-3)/(0.4/8.5)+2.602))
  /*
  EEPROM.begin(sizeof (UnitData));
  systemData.volt400 = 3.08;
  systemData.volt1000 = 2.78;
  EEPROM.put(0, systemData);
  EEPROM.end();
  //*/
}

void loop()
{
  Serial.print( "all " );
  float volts1 = analogRead(A0) / 1023.0 * 3.3;
  delay(25);
  float volts2 = analogRead(A0) / 1023.0 * 3.3;
  delay(25);
  float volts3 = analogRead(A0) / 1023.0 * 3.3;
  float volts = (volts1 + volts2 + volts3) / 3.0;
  Serial.print(volts); 
  Serial.print( "V / " );
  volts = volts / DC_GAIN;
  Serial.print(volts, 3); 
  Serial.print( "V " );

  float delta = systemData.volt400 / DC_GAIN - systemData.volt1000 / DC_GAIN;
  float logHighReference = 3; // 3 for 1000 or 2.903 for 800
  float logCo2 = (volts - systemData.volt400 / DC_GAIN) * (2.602 - logHighReference) / delta + 2.602;
  float ppm = pow(10, logCo2);

  Serial.print("  CO2: ");
  Serial.print(ppm);
  Serial.print("ppm");
  
  Serial.print( "  Time point: " );
  Serial.print(millis() / 60000.0);
  Serial.print("m  ");
  Serial.print(digitalRead(D2));
  Serial.println();

  unsigned long now = millis();
  if (now - lastConnectToServer >= connectToServerInterval) {
    // do not accumulate delay:
    lastConnectToServer = (now / connectToServerInterval) * connectToServerInterval;

    WiFi.forceSleepWake();

    /* Only needed if there is no connection (wrong environment)
    char debugBuffer[30];
    sprintf(debugBuffer, "PPM %d %d %dV\n", (int)((ppm + lastPpm) / 2), ESP.getChipId(), (int)(volts * DC_GAIN  * 100 + 0.5));

    Serial.println(debugBuffer);
    */

    WiFi.begin(systemData.SSID, systemData.pass);

    unsigned long connectStart = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - connectStart > connectTryThreshold) {
        Serial.println("Wifi connect failed (timeout)");
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
        sprintf(transBuffer, "PPM %d %d %dV\n", (int)((ppm + lastPpm) / 2), ESP.getChipId(), (int)(volts * DC_GAIN * 100 + 0.5));

        Serial.print(transBuffer);
        
        client.print(transBuffer);
      }
    }

    // This has 15mA instead of 70mA (above) or 1mA with deep sleep
    WiFi.disconnect(true);
    WiFi.forceSleepBegin();

    lastPpm = ppm;
  }
  
  delay(30000);
}
