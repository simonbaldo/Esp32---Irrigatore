
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
//#include <Wire.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "TIM-28311304"
#define WIFI_PASSWORD "NymyJKNMPN6m2nzq3fRME49z"

// Insert Firebase project API Key
#define API_KEY "AIzaSyDPAyht_0oALhiLzG83Fdr67-1lKP_tCeM"

// Insert Authorized Username and Corresponding Password
#define USER_EMAIL "simonbaldo@gmail.com"
#define USER_PASSWORD "sgpda5n"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "https://esp32-irrigatore-default-rtdb.europe-west1.firebasedatabase.app/"

// Define Firebase objects
FirebaseData stream;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Variables to save database paths
String databasePath;
String tempPath;
String humPath;
String presPath;
String listenerPath;

float temperature;
float humidity;
float pressure;

// Timer variables (send new readings every other minute)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 120000;

// Declare outputs
const int output1 = 2;
const int output2 = 12;
const int slider1 = 13;
const int slider2 = 14;

// Variable to save input message
String message;

// Setting PWM properties
const int freq = 5000;
const int slider1Channel = 0;
const int slider2Channel = 1;
const int resolution = 8;

// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Write float values to the database
void sendFloat(String path, float value){
  if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), value)){
    Serial.print("Writing value: ");
    Serial.print (value);
    Serial.print(" on the following path: ");
    Serial.println(path);
    Serial.println("PASSED");
    Serial.print("PATH: "); Serial.println(fbdo.dataPath());
    Serial.print("TYPE: "); Serial.println(fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: "); Serial.println(fbdo.errorReason());
  }
}

// Callback function that runs on database changes
void streamCallback(FirebaseStream data){
  Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
                data.streamPath().c_str(),
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.eventType().c_str());
  printResult(data); //see addons/RTDBHelper.h
  Serial.println();

  // Get the path that triggered the function
  String streamPath = String(data.dataPath());

  /* When it first runs, it is triggered on the root (/) path and returns a JSON with all key
  and values of that path.So, we can get all values from the database and updated the GPIO
  states, PWM, and message on OLED*/
  if (data.dataTypeEnum() == fb_esp_rtdb_data_type_json){
    FirebaseJson *json = data.to<FirebaseJson *>();
    FirebaseJsonData result;
    if (json->get(result, "/digital/" + String(output1), false)){
      bool state = result.to<bool>();
      digitalWrite(output1, state);
    }
    if (json->get(result, "/digital/" + String(output2), false)){
      bool state = result.to<bool>();
      digitalWrite(output2, state);
    }
    if (json->get(result, "/message", false)){
      String message = result.to<String>();
      //displayMessage(message);
    }
  
    if (json->get(result, "/pwm/" + String(slider1), false)){
      int pwmValue = result.to<int>();
      ledcWrite(slider1Channel, map(pwmValue, 0, 100, 0, 255));
    }
    /*
    if (json->get(result, "/pwm/" + String(slider2), false)){
      int pwmValue = result.to<int>();
      ledcWrite(slider2Channel, map(pwmValue, 0, 100, 0, 255));
    }
    */
  }

  // Check for changes in the digital output values
  if(streamPath.indexOf("/digital/") >= 0){
    // Get string path lengh
    int stringLen = streamPath.length();
    // Get the index of the last slash
    int lastSlash = streamPath.lastIndexOf("/");
    // Get the GPIO number (it's after the last slash "/")
    // UsersData/<uid>/outputs/digital/<gpio_number>
    String gpio = streamPath.substring(lastSlash+1, stringLen);
    Serial.print("DIGITAL GPIO: ");
    Serial.println(gpio);
    // Get the data published on the stream path (it's the GPIO state)
    if(data.dataType() == "int") {
      bool gpioState = data.intData();
      Serial.print("VALUE: ");
      Serial.println(gpioState);
      //Update GPIO state
      digitalWrite(gpio.toInt(), gpioState);
    }
    Serial.println();
  }

  // Check for changes in the slider values
  else if(streamPath.indexOf("/pwm/") >= 0){
    // Get string path lengh
    int stringLen = streamPath.length();
    // Get the index of the last slash
    int lastSlash = streamPath.lastIndexOf("/");
    // Get the GPIO number (it's after the last slash "/")
    // UsersData/<uid>/outputs/PWM/<gpio_number>
    int gpio = (streamPath.substring(lastSlash+1, stringLen)).toInt();
    Serial.print("PWM GPIO: ");
    Serial.println(gpio);
    // Get the PWM Value
    if(data.dataType() == "int"){
      int PWMValue = data.intData();
      Serial.print("VALUE: ");
      Serial.println(PWMValue);
      // Set the duty cycle (PWM) on the corresponding channel
      if (gpio == slider1){
        ledcWrite(slider1Channel, map(PWMValue, 0, 100, 0, 255));
      }
     // if (gpio == slider2){
     //   ledcWrite(slider2Channel, map(PWMValue, 0, 100, 0, 255));
     // }          
    }
  }

  // Check for changes in the message
  else if (streamPath.indexOf("/message") >= 0){
    if (data.dataType() == "string") {
      message = data.stringData();
      Serial.print("MESSAGE: ");
      Serial.println(message);
      // Print on OLED
     // displayMessage(message);
    }
  }
  //This is the size of stream payload received (current and max value)
  //Max payload size is the payload size under the stream path since the stream connected
  //and read once and will not update until stream reconnection takes place.
  //This max value will be zero as no payload received in case of ESP8266 which
  //BearSSL reserved Rx buffer size is less than the actual stream payload.
  Serial.printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());
}

/*
// Callback function that runs on database changes
void streamCallback(FirebaseStream data){
  Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
                data.streamPath().c_str(),
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.eventType().c_str());
  printResult(data); //see addons/RTDBHelper.h
  Serial.println();

  //This is the size of stream payload received (current and max value)
  //Max payload size is the payload size under the stream path since the stream connected
  //and read once and will not update until stream reconnection takes place.
  //This max value will be zero as no payload received in case of ESP8266 which
  //BearSSL reserved Rx buffer size is less than the actual stream payload.
  Serial.printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());
}
*/

void streamTimeoutCallback(bool timeout){
  if (timeout)
    Serial.println("stream timeout, resuming...\n");
  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void setup(){ 
  Serial.begin(115200);
//  pinMode(output1, OUTPUT);
  pinMode(output1, OUTPUT);
  pinMode(output2, OUTPUT);

  // configure PWM functionalitites
  ledcSetup(slider1Channel, freq, resolution);
  //ledcSetup(slider2Channel, freq, resolution);
  // attach the channels to the GPIOs to be controlled
  ledcAttachPin(slider1, slider1Channel);
  //ledcAttachPin(slider2, slider2Channel);

  // Init BME sensor, OLED, and WiFi
  initWiFi();

  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }

  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path with user UID
  //String s = "/UserdData/";
  databasePath = "/UsersData/" + uid;
  //databasePath = s + uid;

  // Define database path for sensor readings
  tempPath = databasePath + "/sensor/temperature"; // --> UsersData/<user_uid>/sensor/temperature
  humPath = databasePath + "/sensor/humidity"; // --> UsersData/<user_uid>/sensor/humidity
  presPath = databasePath + "/sensor/pressure"; // --> UsersData/<user_uid>/sensor/pressure

  // Update database path for listening
  listenerPath = databasePath + "/outputs/";

  // Streaming (whenever data changes on a path)
  // Begin stream on a database path --> UsersData/<user_uid>/outputs
  if (!Firebase.RTDB.beginStream(&stream, listenerPath.c_str()))
    Serial.printf("stream begin error, %s\n\n", stream.errorReason().c_str());

  // Assign a calback function to run when it detects changes on the database
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);

  delay(2000);
}

void loop(){
  // Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    // Get latest sensor readings
    //temperature = bme.readTemperature();
    //temperature = 1.8 * bme.readTemperature() + 32;
    //humidity = bme.readHumidity();
    //pressure = bme.readPressure()/100.0F;

    // Send readings to database:
    //sendFloat(tempPath, temperature);
    //sendFloat(humPath, humidity);
    //sendFloat(presPath, pressure);
  }
}
