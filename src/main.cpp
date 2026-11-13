// *********************************************************************
// COMP-10184 - Project One
// Authors:  Ebenezer Fanta, Dragana Acamovic
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
int COLLECTION_TIME = 100;
int SAMPLES_PER_SECOND = 8000;

//global sensor data to be sent to html page
int variableResistorPercentage = 0;
float microphoneVal = 0;
float DB_OFFSET = 15; //tested from phone decibel tracker to get a more accurate representation of the real decibels
bool decibelMode = true;

//error checking
bool micOk = true;
bool resistorOk = true;
unsigned long lastWifiCheck = 0;
int lastResistorVal = -1; 




// *********************************************************************
// setup()
//



void setup() {
  String sStatus;

  // setup serial debug port
  Serial.begin(115200);
  delay(5000);

  // init filesystem
  if(!LittleFS.begin(true, "/littlefs")){
    Serial.println("Error initializing LittleFS!");
    while(1) delay(1);
  } 
  Serial.println("File System Initialized");

	File root = LittleFS.open("/");
  	File file = root.openNextFile();
  	while (file) {
    	Serial.printf("Found file: %s\n", file.name());
		file = root.openNextFile();
	}

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
  webServer.serveStatic("/index.css", LittleFS, "/index.css");
  webServer.serveStatic("/index.js", LittleFS, "/index.js");
  webServer.serveStatic("/images/", LittleFS, "/images/");

  // raw text and HTTP Response=200 (OK) response.

  webServer.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"resistor\":" + String(variableResistorPercentage) +
                  ",\"microphone\":" + String(microphoneVal) + 
                  ",\"decibelMode\":" + (decibelMode ? "true" : "false") +
                  ",\"micOk\":" + (micOk ? "true" : "false") +            
                  ",\"resistorOk\":" + (resistorOk ? "true" : "false") + 
                  "}";
                  
    request->send(200, "application/json", json);
  });


  webServer.on("/changeMode", HTTP_GET, [](AsyncWebServerRequest *request){
	decibelMode = !decibelMode;
	request -> send(200, "text/plain", "The mode was changed.");
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







//will only give a relative decibel rating
void getDecibels(){
	unsigned long start = millis();
	int minRaw = 4095; //min Raw set to max value so its guaranteed to change
	int maxRaw = 0; //max Raw set to min value so its guaranteed to change
	
	//collects data from microphone for specified collection time
	while (millis() - start < COLLECTION_TIME){
		int raw = analogRead(PIN_MIC);
		if (raw > maxRaw) maxRaw = raw;
		if (raw < minRaw) minRaw = raw;
	}

	int peakToPeak = maxRaw - minRaw;

	int midpoint = (minRaw + maxRaw) / 2;
	if(midpoint < 1000 || midpoint > 2100){
		micOk = false;
		return;
	}
	micOk = true;

	if(peakToPeak < 1) peakToPeak = 1;
	microphoneVal = 20 * log10(peakToPeak / 1.0) - DB_OFFSET;
	if(microphoneVal < 0) microphoneVal = 0;
	Serial.printf("Decibels: %.1f dB\n", microphoneVal);
}

void getHertz(){
	int samplesCollected = 0;
	int sampleSize = (COLLECTION_TIME * SAMPLES_PER_SECOND) / 1000;
	unsigned long interval = 1000000 / SAMPLES_PER_SECOND;

	bool prev = false;
	int crossings = 0;
	long sum = 0;
	unsigned long currentTime = micros();
	while(samplesCollected < sampleSize){
		if(micros() - currentTime >= interval){
			int raw = analogRead(PIN_MIC);
			sum += raw;
			bool curr = ( raw > 1550 ); 
			if(samplesCollected > 0 && !prev && curr) crossings++;
			prev = curr;
			currentTime += interval;
			samplesCollected++;
		}
	}


	int average = sum / sampleSize;
	if (average < 1000 || average > 2100) {
		micOk = false;
		return;
	}
	micOk = true;

	microphoneVal = crossings / (COLLECTION_TIME / 1000.0); 
	Serial.printf("Frequency: %.1f Hz\n", microphoneVal);
  
}
 

//read the resistor val and return the percentage 
void readResistor() {
    int minVal = 4095;
    int maxVal = 0;
    long total = 0;

    // Take 20 readings
    for (int i = 0; i < 20; i++) {
        int raw = analogRead(PIN_VARIABLE_RES);

        if (raw < minVal) minVal = raw;
        if (raw > maxVal) maxVal = raw;

        total += raw;
        delayMicroseconds(500);
    }

    int difference = maxVal - minVal;

    // Detect unstable readings
    resistorOk = difference < 150;

    if (!resistorOk) {
        Serial.println("Dial may be disconnected!");
        return;
    }

    int average = total / 20;

    float percent = (average / 4095.0) * 100.0;

    variableResistorPercentage = round(percent);

    Serial.printf("Dial: %d%% | Fluctuation: %d\n",
                  variableResistorPercentage, difference);
}
// *********************************************************************
// loop ()
//
void loop() {

	//check if we are connected to wifi and try to reconnect if not
	if (WiFi.status() != WL_CONNECTED && millis() - lastWifiCheck > 5000){
		lastWifiCheck = millis();
		Serial.println("WiFi lost - attempting to reconnect...");
		WiFi.reconnect();
	}

	readResistor();
	
	decibelMode ? getDecibels() : getHertz();

//   Serial.print("ESP32 IP Address: ");
    // Serial.println(WiFi.localIP());
  // delay(1000) => is bad
}
