// *********************************************************************
// COMP-10184 - Project One
// Authors:  Ebenezer Fanta, Dragana Acemovic
// Web Server + LittleFS tutorial
//

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <math.h>

// WiFi credentials are in this file.  Change to suit your context!
#include "secrets.h"

// *********************************************************************
// constants
#define VERSION     "2.0"

#define PIN_VARIABLE_RES 36
#define PIN_MIC 39 

// *********************************************************************
// data

// web server hosted internally to the IoT device.  Listen on TCP port 80,
// which is the default HTTP port.
AsyncWebServer    webServer(80);

//define update value for any data collection so that we can sync it roughly with the http server
int COLLECTION_TIME = 50;

//global sensor data to be sent to html page
int variableResistorPercentage = 0;
float microphoneVal = 0;
bool decibelMode = true;


//structs
struct MicVoltage {
	int max;
	int min;
};

// *********************************************************************
// setup()
//



void setup() {
  String sStatus;

  // setup serial debug port
  Serial.begin(115200);

  // init filesystem
  if(!LittleFS.begin(true, "/littlefs")){
    Serial.println("Error initializing LittleFS!");
    while(1) delay(1);
  } 
  Serial.println("File System Initialized");

  // attempt to connect to WiFi network
  Serial.printf("\nConnecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // set up webserver handlers...
  
  // static pages
  webServer.serveStatic("/index.html", LittleFS, "/index.html");

  // raw text and HTTP Response=200 (OK) response.
  webServer.on("/index", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(variableResistorPercentage));
  });



  // unknown URL's (HTTP Response = 404)
  webServer.onNotFound([](AsyncWebServerRequest *request){
    Serial.println("URL not found: " + request->url());
    request->send(404, "text/plain", "The content you are looking for was not found.");
  
  });

  // redirect root request -> index.html
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Redirecting to /index.html");
    request->redirect("/index.html");
  });

  // start web server
  webServer.begin();

  // print our IP address.
  Serial.println("\nWeb server started, open: " + WiFi.localIP().toString() +  " in a web browser\n");
}





MicVoltage microphoneData(){
	unsigned long start = millis();
	int minRaw = 4095; //min Raw set to max value so its guaranteed to change
	int maxRaw = 0; //max Raw set to min value so its guaranteed to change
	
	//collects data from microphone for specified collection time
	while (millis() - start < COLLECTION_TIME){
		int raw = analogRead(PIN_MIC);
		if (raw > maxRaw) maxRaw = raw;
		if (raw < minRaw) minRaw = raw;
	}

	return {maxRaw, minRaw};
}

float getDecibels(){
	MicVoltage voltage = microphoneData();
	microphoneVal = 20 * log10(voltage.max / voltage.min);
}

float getHertz(){

}
 

//read the resistor val and return the percentage 
void readResistor(){
  int sensorVal = analogRead(PIN_VARIABLE_RES);

  float percent = (sensorVal / 4095.0) * 100.0; 

  variableResistorPercentage = round(percent); 
}

// *********************************************************************
// loop ()
//
void loop() {

  readResistor();
  
  decibelMode ? getDecibels() : getHertz();
  // delay(1000) => is bad
}
