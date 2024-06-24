//Includes new icons
#include <M5UnitLCD.h>
#include <M5UnitOLED.h>
#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "WiFi.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
// #include <NTPClient.h>          // Time Protocol Libraries
// #include <WiFiUdp.h>            // Time Protocol Libraries

// Audio varibales
static constexpr const size_t record_number = 256;
static constexpr const size_t record_length = 300;
static constexpr const size_t record_size = record_number * record_length;
static constexpr const size_t record_samplerate = 16000;
static int16_t prev_y[record_length];
static int16_t prev_h[record_length];
static size_t rec_record_idx = 2;
static size_t draw_record_idx = 0;
static int16_t *rec_data;
int volumeProx = 255;

// bluetooth variables
BLEServer *bleServer;
BLEService *bleService;
BLECharacteristic *bleCharacteristic;
BLECharacteristic *bleCharacteristic2;
static BLERemoteCharacteristic *bleRemoteCharacteristic;
bool deviceConnected = false;
bool previouslyConnected = false;
int timer = 0;

// See the following for generating UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID "a02a5614-7362-4663-8376-54ec54914a70"
#define CHARACTERISTIC_UUID "882711e8-f7cb-4351-8066-5fd909d4c68b"
#define CHARACTERISTIC_UUID2 "674b2304-53d1-4517-81f1-6aab35edc0d4"

// BLE Broadcast Name
static String BLE_BROADCAST_NAME = "Jereds M5Core2024";

// Other variables
int lastTime = 0;

const String URL_GCF_UPLOAD = "https://us-west2-egr423-2024-sr.cloudfunctions.net/sensor-data";

String wifiNetworkName = "CBU-LANCERS";
String wifiPassword = "LiveY0urPurp0se";

//Screen Coordinates
int centerX = 120;
int centerY = 160;
int micWidth = 20;
int micHeight = 40;

//Triangle Coordinates
// Calculate the vertices of the triangle (play icon)
int triSize = 50;
int x1 = centerX - triSize / 3;  // Left vertex (middle height)
 // Top vertex
int x2 = centerX - triSize / 3;  // Left vertex (middle height)
int y2 = centerY + triSize / 2;  // Bottom vertex
int x3 = centerX + (2 * triSize / 3); // Right vertex (point of the triangle)
int yy1 = centerY - triSize / 2;

// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP);

bool redraw = false;

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods
///////////////////////////////////////////////////////////////
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        previouslyConnected = true;
        Serial.println("Device connected...");
        redraw = true;
    }
    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }
};

///////////////////////////////////////////////////////////////
// BLECharacteristic Callback Methods
///////////////////////////////////////////////////////////////
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    // Callback function to support a read request.
    void onRead(BLECharacteristic* pCharacteristic) {
        String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
        String chraracteristicValue = pCharacteristic->getValue().c_str();
        // Serial.printf("Client JUST read from %s: %s", characteristicUUID.c_str(), chraracteristicValue.c_str());
    }

    // Callback function to support a write request.
    void onWrite(BLECharacteristic* pCharacteristic) {
        // Get the characteristic enum and print for logging
        String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
        String chraracteristicValue = pCharacteristic->getValue().c_str();
        // Serial.printf("Client JUST wrote to %s: %s", characteristicUUID.c_str(), chraracteristicValue.c_str());

        // TODO: Take action by checking if the characteristicUUID matches a known UUID
		if (characteristicUUID.equalsIgnoreCase(CHARACTERISTIC_UUID)) {
			// The characteristicUUID that just got written to by a client matches the known
			// CHARACTERISTIC_UUID (assuming this constant is defined somewhere in our code)
		}
    }

    // Callback function to support a Notify request.
    void onNotify(BLECharacteristic* pCharacteristic) {
        String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
        // Serial.printf("Client JUST notified about change to %s: %s", characteristicUUID.c_str(), pCharacteristic->getValue().c_str());
    }

    // Callback function to support when a client subscribes to notifications/indications.
    void onSubscribe(BLECharacteristic* pCharacteristic, uint16_t subValue) {
    }

    // Callback function to support a Notify/Indicate Status report.
    void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
        // Print appropriate response
		String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
        switch (s) {
            case SUCCESS_INDICATE:
                // Serial.printf("Status for %s: Successful Indication", characteristicUUID.c_str());
                break;
            case SUCCESS_NOTIFY:
                Serial.printf("Status for %s: Successful Notification", characteristicUUID.c_str());
                break;
            case ERROR_INDICATE_DISABLED:
                Serial.printf("Status for %s: Failure; Indication Disabled on Client", characteristicUUID.c_str());
                break;
            case ERROR_NOTIFY_DISABLED:
                Serial.printf("Status for %s: Failure; Notification Disabled on Client", characteristicUUID.c_str());
                break;
            case ERROR_GATT:
                Serial.printf("Status for %s: Failure; GATT Issue", characteristicUUID.c_str());
                break;
            case ERROR_NO_CLIENT:
                Serial.printf("Status for %s: Failure; No BLE Client", characteristicUUID.c_str());
                break;
            case ERROR_INDICATE_TIMEOUT:
                Serial.printf("Status for %s: Failure; Indication Timeout", characteristicUUID.c_str());
                break;
            case ERROR_INDICATE_FAILURE:
                Serial.printf("Status for %s: Failure; Indication Failure", characteristicUUID.c_str());
                break;
        }
    }
};

// recording methods
void recordAudio();
void playAudio();
void increaseNoiseFilterLevel();
// long mapper(long x, long in_min, long in_max, long out_min, long out_max);
void broadcastBleServer();
void drawScreenTextWithBackground(String text, int backgroundColor);
bool gcfGetWithHeader(String serverUrl, String userId);
String generateM5DetailsHeader(String userId);
int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders);

long mapper(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setup(void)
{
  auto cfg = M5.config();

  M5.begin(cfg);

  M5.Lcd.setTextSize(3);

  // Initialize M5Core2 as a BLE server
  Serial.print("Starting BLE...");
  BLEDevice::init(BLE_BROADCAST_NAME.c_str());

  // Broadcast the BLE server
  drawScreenTextWithBackground("Initializing BLE...", TFT_CYAN);
  broadcastBleServer();
  drawScreenTextWithBackground("Broadcasting as BLE server named:\n\n" + BLE_BROADCAST_NAME, TFT_BLUE);

  M5.Imu.init();

  M5.Display.startWrite();

  // if (M5.Display.width() > M5.Display.height())
  // {
  //   M5.Display.setRotation(M5.Display.getRotation()^1);
  // }

  // M5.Lcd.clear();
  M5.Display.setCursor(0, 0);
  // M5.Display.print("REC");
  rec_data = (typeof(rec_data))heap_caps_malloc(record_size * sizeof(int16_t), MALLOC_CAP_8BIT);
  memset(rec_data, 0 , record_size * sizeof(int16_t));
  M5.Speaker.setVolume(volumeProx);

  /// Since the microphone and speaker cannot be used at the same time, turn off the speaker here.
  M5.Speaker.end();
  M5.Mic.begin();
  
  // Init time connection
  // timeClient.begin();
  // timeClient.setTimeOffset(3600 * -7);
}

bool previousRecording = false;
bool recording = false;
int recordingTimer = 0;
int otherTimer = 5;
void loop(void)
{
  if (deviceConnected) {
    M5.update();

    float accX;
    float accY;
    float accZ;
    M5.Imu.getAccelData(&accX, &accY, &accZ);
    accX *= 9.8;
    accY *= 9.8;
    accZ *= 9.8;

    if ((accX > 20 || accX < -20 || M5.BtnA.isPressed()) && !recording) {
      M5.Display.clear();
        M5.Lcd.fillRect(0, 0, 240, 30, BLACK);
        //M5.Lcd.fillScreen(TFT_BLACK); // Clear the screen
        M5.Lcd.fillCircle(120, 160, 50, TFT_DARKGRAY);
        M5.Lcd.fillCircle(120, 160, 35, TFT_RED);
      recording = true;
      lastTime = millis();
      recordingTimer = millis();
      M5.Display.setCursor(0,0);
      M5.Display.printf("RECORDING, %i sec(s) left", otherTimer); 
    }

    if ((accZ > 40 || accZ < -40 || M5.BtnB.isPressed()) && !recording) {
      previousRecording = true;
      redraw = true;

      playAudio();

      // Deinitialize M5Core2 as a BLE server
      Serial.print("Ending BLE...");
      BLEDevice::stopAdvertising();
      BLEDevice::deinit(true);

      ///////////////////////////////////////////////////////////
      // Connect to WiFi
      ///////////////////////////////////////////////////////////
      M5.Display.clear();
      M5.Display.setCursor(0,0);
      M5.Display.println("LOGGING RECORDING");
      M5.Lcd.fillCircle(120, 160, 50, TFT_DARKGRAY);
      M5.Lcd.fillTriangle(80, 160, 120, 120, 160, 160, TFT_SKYBLUE);
      M5.Lcd.fillRect(100, 160, 40, 30, TFT_SKYBLUE);
      WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
      Serial.printf("Connecting");
      while (WiFi.status() != WL_CONNECTED)
      {
          delay(500);
          Serial.print(".");
      }
      M5.Display.println("Connected to WiFi network");
      Serial.print("\n\nConnected to WiFi network with IP address: ");
      Serial.println(WiFi.localIP());
      gcfGetWithHeader(URL_GCF_UPLOAD, "SimeonRinkenberger");
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);

      // Initialize M5Core2 as a BLE server
      // Serial.print("Starting BLE...");
      // BLEDevice::init(BLE_BROADCAST_NAME.c_str());
      // broadcastBleServer();
    }

    if (recording) {
      if (millis() - lastTime > 1000) {
        //M5.Display.clear();
        M5.Lcd.fillRect(0, 0, 240, 30, BLACK);
        //M5.Lcd.fillScreen(TFT_BLACK); // Clear the screen
        M5.Lcd.fillCircle(120, 160, 50, TFT_DARKGRAY);
        M5.Lcd.fillCircle(120, 160, 35, TFT_RED);
        //M5.Lcd.setCursor(105, 150);
        //M5.Lcd.setTextColor(TFT_WHITE);
        //M5.Lcd.print("REC");


        otherTimer -= 1;
        lastTime = millis();
        M5.Display.setCursor(0,0);
        //M5.Display.printf("%i sec", otherTimer); 
        M5.Display.printf("RECORDING, %i sec(s) left", otherTimer);
      }

      recordAudio();
      if (millis() - recordingTimer > 5000) {
        recording = false;
        redraw = true;
        otherTimer = 5;
      }
    }

    if (redraw) {
      M5.Lcd.fillScreen(TFT_BLACK); // Clear the screen
      M5.Lcd.setTextColor(TFT_WHITE); // Set text color to white
      M5.Lcd.setTextSize(2); // Set text size to 2

      M5.Lcd.setCursor(0, 0); // Set cursor position
      M5.Lcd.println("Shake/Press BtnA \nto record for 5s\n");
      M5.Lcd.println("Twist/Press BtnB \nto hear the \nrecording"); // Print text on the screen
      //M5.Lcd.println("Trigger prox to \nrecord for 5 sec \n\nPress button B to \nhear the recording"); // Print text on the screen
      redraw = false;
    }
  }

  if (M5.BtnC.isPressed()) {
    Serial.println("Button C pressed");
    redraw = true;
  }
}

void recordAudio() {
  if (M5.Mic.isEnabled()) {
    static constexpr int shift = 6;

    // Check if previousRecording flag is set
    if (previousRecording) {
      // Reset the recording index and clear the recorded data buffer
      rec_record_idx = 0;
      draw_record_idx = 0;
      memset(rec_data, 0, record_size * sizeof(int16_t));
      previousRecording = false;  // Reset the flag after clearing
    }

    auto data = &rec_data[rec_record_idx * record_length];
    if (M5.Mic.record(data, record_length, record_samplerate))
    {
      data = &rec_data[draw_record_idx * record_length];

      if (++draw_record_idx >= record_number) { draw_record_idx = 0; }
      if (++rec_record_idx >= record_number) { rec_record_idx = 0; }
    }
  }
}

void increaseNoiseFilterLevel() {
  auto cfg = M5.Mic.config();
  cfg.noise_filter_level = (cfg.noise_filter_level + 8) & 255;
  M5.Mic.config(cfg);
}

void playAudio() {
  if (M5.Speaker.isEnabled()) {
    M5.Display.clear();

    /// Since the microphone and speaker cannot be used at the same time, turn off the microphone here.
    M5.Mic.end();
    M5.Speaker.begin();

    M5.Display.setCursor(0,0);
    M5.Display.print("PLAYING AUDIO \nRECORDING");
    M5.Lcd.fillCircle(centerX, centerY, 50, TFT_DARKGRAY);
    M5.Lcd.fillTriangle(x1, yy1, x2, y2, x3, centerY, TFT_GREEN);
    int start_pos = rec_record_idx * record_length;
    if (start_pos < record_size)
    {
      M5.Speaker.playRaw(&rec_data[start_pos], record_size - start_pos, record_samplerate, false, 1, 0);
    }
    if (start_pos > 0)
    {
      M5.Speaker.playRaw(rec_data, start_pos, record_samplerate, false, 1, 0);
    }
    do
    {
      delay(10);
      M5.update();

      if (deviceConnected) {
        // 2 - Read the characteristic's value as a string (which can be written from a client)
        std::string readValue = bleCharacteristic->getValue();
        // Serial.printf("The new characteristic value as a STRING is: %s\n", readValue.c_str());
        String str = readValue.c_str();
        volumeProx = str.toInt();
        // Serial.println(volumeProx);
      } else if (previouslyConnected) {
        drawScreenTextWithBackground("Disconnected. Reset M5 device to reinitialize BLE.", TFT_RED); // Give feedback on screen
        timer = 0;
      }

      // int prox = I2C_RW::readReg8Addr16Data(VCNL_REG_PROX_DATA, 2, "to read proximity data", false);
      // Serial.printf("Proximity: %d\n", prox);
      // uint16_t volume = mapper(prox, 0, 10, 30, 255);
      uint16_t volume = mapper(volumeProx, 0, 10, 30, 255);
      M5.Speaker.setVolume(volume);
    } while (M5.Speaker.isPlaying());

    /// Since the microphone and speaker cannot be used at the same time, turn off the speaker here.
    M5.Speaker.end();
    M5.Mic.begin();
  }
}

void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(0,0);
    M5.Lcd.println(text);
}

///////////////////////////////////////////////////////////////
// This code creates the BLE server and broadcasts it
///////////////////////////////////////////////////////////////
void broadcastBleServer() {    
    // Initializing the server, a service and a characteristic 
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new MyServerCallbacks());
    bleService = bleServer->createService(SERVICE_UUID);
    bleCharacteristic = bleService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
    bleCharacteristic->setValue("Hello BLE World from Dr. Dan!");
    bleCharacteristic->setCallbacks(new MyCharacteristicCallbacks());


    bleCharacteristic2 = bleService->createCharacteristic(CHARACTERISTIC_UUID2,
        BLECharacteristic::PROPERTY_READ |
        // BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    bleCharacteristic2->setValue("Hello BLE World from Dr. Dan2!");
    bleCharacteristic2->setCallbacks(new MyCharacteristicCallbacks());
    bleService->start();

    // Start broadcasting (advertising) BLE service
    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(SERVICE_UUID);
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinPreferred(0x12); // Use this value most of the time 
    // bleAdvertising->setMinPreferred(0x06); // Functions that help w/ iPhone connection issues 
    // bleAdvertising->setMinPreferred(0x00); // Set value to 0x00 to not advertise this parameter
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined...you can connect with your phone!"); 
}

bool gcfGetWithHeader(String serverUrl, String userId) {
  // Allocate arrays for headers
	const int numHeaders = 1;
  String headerKeys [numHeaders] = {"M5-Details"};
  String headerVals [numHeaders];

  // Add formatted JSON string to header
  headerVals[0] = generateM5DetailsHeader(userId);
  
  // Attempt to post the file
  int resCode = httpGetWithHeaders(serverUrl, headerKeys, headerVals, numHeaders);
  
  // Return true if received 200 (OK) response
  return (resCode == 200);
}

String generateM5DetailsHeader(String userId) {
  // Allocate M5-Details Header JSON object
  JsonDocument objHeaderM5Details; //DynamicJsonDocument  objHeaderGD(600);
    
  // Get current time as timestamp of last update
  // timeClient.update();
  // unsigned long epochTime = timeClient.getEpochTime();
  // unsigned long long epochMillis = ((unsigned long long)epochTime)*1000 + (3600*7)*1000;
  // struct tm *ptm = gmtime ((time_t *)&epochTime);
  // Serial.printf("\nCurrent Time:\n\tEpoch (ms): %llu", epochMillis);
  // Serial.printf("\n\tFormatted: %d/%d/%d ", ptm->tm_mon+1, ptm->tm_mday, ptm->tm_year+1900);
  // Serial.printf("%02d:%02d:%02d%s\n\n", timeClient.getHours() % 12, timeClient.getMinutes(), timeClient.getSeconds(), timeClient.getHours() < 12 ? "AM" : "PM");

  // String time = String(ptm->tm_year+1900) + "-" + String(ptm->tm_mon+1) + "-" + String(ptm->tm_mday) + " " + String(timeClient.getHours()) + ":" + String(timeClient.getMinutes()) + ":" + String(timeClient.getSeconds());

  // // // add recording details
  // JsonObject objRecordingDetails = objHeaderM5Details.createNestedObject("recordingDetails");
  // objRecordingDetails["recordData"] = "Recorded at " + time;

  // Add Other details
  JsonObject objOtherDetails = objHeaderM5Details.createNestedObject("otherDetails");
  objOtherDetails["userId"] = userId;

  // Convert JSON object to a String which can be sent in the header
  // size_t jsonSize = measureJson(objHeaderM5Details) + 1;
  // char cHeaderM5Details [jsonSize];
  String strHeaderM5Details;
  serializeJson(objHeaderM5Details, strHeaderM5Details);
  // String strHeaderM5Details = cHeaderM5Details;
  // Serial.println(strHeaderM5Details); // Debug print

  // Return the header as a String
  return strHeaderM5Details;
}

int httpGetWithHeaders(String serverURL, String *headerKeys, String *headerVals, int numHeaders) {
  // Make GET request to serverURL
  HTTPClient http;
  http.begin(serverURL.c_str());
    
	////////////////////////////////////////////////////////////////////
	// TODO 5: Add all the headers supplied via parameter
	////////////////////////////////////////////////////////////////////
    for (int i = 0; i < numHeaders; i++)
        http.addHeader(headerKeys[i].c_str(), headerVals[i].c_str());
    
    // Post the headers (NO FILE)
    int httpResCode = http.GET();

    // Print the response code and message
    Serial.printf("HTTP%scode: %d\n%s\n\n", httpResCode > 0 ? " " : " error ", httpResCode, http.getString().c_str());

    // Free resources and return response code
    http.end();
    return httpResCode;
}
