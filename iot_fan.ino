



//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ loading libarys §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
#include <Wire.h>
#include <ESP8266WiFi.h>
#include "SparkFun_SCD30_Arduino_Library.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>


//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ dummy functions §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
void getData();
void sndData();
void wificonnect();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();

//§§§§§§§§§§§§§§§§§§§§§§§§§§§§ settings for data transmission §§§§§§§§§§§§§§§§§§§§§§§
int send_interval = 10000; // time between messages in ms


//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ defining wifi acess §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
#ifndef STASSID
#define STASSID "***"
#define STAPSK  "***"
#endif

const char* ssid     = STASSID;
const char* password = STAPSK;

String mac_address = "";
int numWiFiRestarts = 0;


//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ defining mqtt connection §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
const char* mqtt_server = "***";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[100];
int value = 0;

int numMQTTRestarts = 0;

//§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ defining air quality §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
SCD30 airSensor;

  //§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ Fan initialisation §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§

  int fanPin     = D3; // digital PWM pin 9

void setup()
{


  Serial.begin(115200);

  //§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ defining buffer 4 JSON object §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
  StaticJsonDocument<200> doc;

  //§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ connect to wifi §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
  wificonnect();

  //§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ MQTT initalisation §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  //§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ CO2-Sernsor initalisation §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§

  Wire.begin();
  airSensor.begin(); //This will cause readings to occur every two seconds


  //§§§§§§§§§§§§§§§§§§§§§§§§§§§§§ Fan setup §§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§§

   pinMode(fanPin, OUTPUT);   // OCR2B sets duty cycle
   analogWriteFreq(31000);

}

void loop()
{

  if (WiFi.status() != WL_CONNECTED) {
    wificonnect();
  }

  //wenn mqtt client nicht verbunden ist, solang versuchen wiederzuverbinden bis erfolgreich
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > send_interval) {
    lastMsg = now;

    getData();

  }
}



void getData() {

  if (airSensor.dataAvailable())
  {

    sndData("co2", "ppm", airSensor.getCO2()); // Abrufen der CO2 Konzentration und übergabe an die senden Funktion

    sndData("temp", "°C", round(airSensor.getTemperature() * 10) / 10); // Abrufen der Temperatur und übergabe an die senden Funktion

    sndData("relHum", "%", round(airSensor.getHumidity() * 10) / 10); // Abrufen der relativen Luftfeuchte und übergabe an die senden Funktion

  }

  // Systemdaten ermitteln und senden
  sndData("upTime", "sec", millis() / 1000); // Abrufen der Zeit seit dem letzen Reset und übergabe an die Sendenfunktion

  sndData("WiFi_Restarts", "-", numWiFiRestarts); // Abrufen der WiFi Restarts seit dem letzen Reset und übergabe an die Sendenfunktion

  sndData("MQTT_Restarts", "-", numMQTTRestarts); // Abrufen der MQTT Restarts seit dem letzen Reset und übergabe an die Sendenfunktion
}

void sndData(String sensorID, String unit, float value) {

  StaticJsonDocument<200> doc;
  doc["macAddresse"] = mac_address;
  doc["sensorID"] = sensorID;
  doc["unit"] = unit;
  doc["value"] = value;

  serializeJson(doc, msg);

  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("data", msg);
}



void wificonnect() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC-Adress: ");
  mac_address = WiFi.macAddress();
  Serial.println(mac_address);

  numWiFiRestarts += 1; // Zähler für WiFi Restarts ein hochsetzen
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.print(length);
  Serial.print(" ");
  char json[100];


  for (int i = 0; i < length; i++) {
    json[i]  =  (char)payload[i];
  }
  Serial.println(json);


  StaticJsonDocument<200> doc;
  // Deserialize the JSON document
  deserializeJson(doc, json);

  String recivingMac = doc["mac_address"];
  int fanSpeed = doc["fanSpeed"];
  Serial.println(recivingMac);
  Serial.println(fanSpeed);
  if (recivingMac == mac_address){
  int pwm = map(fanSpeed, -100, 100, 60, 1023); // Verteilt den PWM Wert über den Messbereich des Potis
  analogWrite(fanPin, pwm );
  Serial.println(pwm);

  }





}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...

      numMQTTRestarts += 1; // Zähler für MQTT Restarts ein hochsetzen

      client.subscribe("fan");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
