/* Volume Control for the AV Receiver Yamaha RX-V677 
 * A ESP 8266 is the core. 
 * On Pin is a ws2811 LED. The LED gives Signals to the user.
 */

#include <Arduino.h>
#include <Encoder.h>
#include <Adafruit_NeoPixel.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

Encoder myEnc(5, 4);
ESP8266WiFiMulti WiFiMulti;
HTTPClient http;

int pressPin = 0;

String avState    = "";
String oldAvState   = "";

String playState    = "Pause";
String oldPlayState   = "Pause";

long pressTime = 0;
long releaseTime = 0;

int neoPixelPin = 3;
int numPixel = 1;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(numPixel, neoPixelPin, NEO_GRB + NEO_KHZ800);

void pressButton() {
  pressTime = millis();
  attachInterrupt(pressPin, releaseButton, RISING);
}

void releaseButton() {
  releaseTime = millis();
  int pressDuration = releaseTime - pressTime;
  if (pressDuration <= 1000) {
    shortPress();
  } else {
    longPress();
  }
  attachInterrupt(pressPin, pressButton, FALLING);
}

void shortPress() {
  if (playState == "Pause") {
    playState = "Play";
  } else {
    playState = "Pause";
  }
}

void longPress() {
  if (avState == "On") {
    avState = "Standby";
  } else {
    avState = "On";
  }
}

void changeLedColor(String avState) {
  if (avState == "On") {
    pixels.setPixelColor(0, pixels.Color(0,150,0));
    pixels.show();
  } else {
    pixels.setPixelColor(0, pixels.Color(150,0,0));
    pixels.show();
  }
}

String getXMLValue (String tag, String message) {
  int tagPosition = message.indexOf("<" + tag + ">");
  int tagEndPosition = message.indexOf("</" + tag + ">");
  int tagLength = tag.length() + 2;
  return message.substring(tagPosition + tagLength, tagEndPosition);
}

void setup() {
  Serial.begin(9600);

  pinMode(0, INPUT);
  attachInterrupt(pressPin, pressButton, FALLING);
  
  for(uint8_t t = 4; t > 0; t--) {
      Serial.printf("[SETUP] WAIT %d...\n", t);
      Serial.flush();
      delay(1000);
  }

  WiFiMulti.addAP("****", "****");
  pixels.begin();
}

long oldPosition  = myEnc.read();
long volume       = 0;
long loopTime     = 0;
long timeDiff     = 0;

void loop() {
  long newPosition = myEnc.read();
  timeDiff = millis() - loopTime;

  if((WiFiMulti.run() == WL_CONNECTED)) {
    if (newPosition != oldPosition) {
      oldPosition = newPosition;
      volume = (newPosition*5)-300;

      http.begin("http://192.168.2.107:80/YamahaRemoteControl/ctrl");
      http.addHeader("Content-Type", "text/xml; charset=UTF-8");
      http.POST("<YAMAHA_AV cmd=\"PUT\"><Main_Zone><Volume><Lvl><Val>" + String(volume) + "</Val><Exp>1</Exp><Unit>dB</Unit></Lvl></Volume></Main_Zone></YAMAHA_AV>");
      http.end();
    }

    if (avState != oldAvState) {
      oldAvState = avState;
      
      http.begin("http://192.168.2.107:80/YamahaRemoteControl/ctrl");
      http.addHeader("Content-Type", "text/xml; charset=UTF-8");
      http.POST("<YAMAHA_AV cmd=\"PUT\"><Main_Zone><Power_Control><Power>" + avState + "</Power></Power_Control></Main_Zone></YAMAHA_AV>");
      http.end();

      changeLedColor(avState);
    }

    if (playState != oldPlayState) {
      oldPlayState = playState;
      
      http.begin("http://192.168.2.107:80/YamahaRemoteControl/ctrl");
      http.addHeader("Content-Type", "text/xml; charset=UTF-8");
      http.POST("<YAMAHA_AV cmd=\"PUT\"><Main_Zone><Play_Control><Playback>" + playState + "</Playback></Play_Control></Main_Zone></YAMAHA_AV>");
      http.end();
    }

    if (timeDiff >= 10000) {
      loopTime = millis();

      http.begin("http://192.168.2.107:80/YamahaRemoteControl/ctrl");
      http.addHeader("Content-Type", "text/xml; charset=UTF-8");
      http.POST("<YAMAHA_AV cmd=\"GET\"><Main_Zone><Basic_Status>GetParam</Basic_Status></Main_Zone></YAMAHA_AV>");
      String response = http.getString();
      http.end();

      avState = getXMLValue("Power",response);
      if (avState != oldAvState) {
        oldAvState = avState;
        changeLedColor(avState);
      }
      
      String VolumeStats = getXMLValue("Volume",response);
      String LvlStats = getXMLValue("Lvl",VolumeStats);
      volume = getXMLValue("Val",LvlStats).toInt();
      newPosition = (volume-300)/5;
      oldPosition = newPosition;      
    }
  }
}

