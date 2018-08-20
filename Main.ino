/*
  /////////////Physical pins on NodeMCU 1.0 (ESP8266 12E)
  D2 - PIR Sensor
  D5 - Gerkon (Door)
  D6 - DHT22
  D7 - Buzzer

  //////////////Virtuals pins of BLYNK//////////////
  V1  - Widget LED
  V2  - Widget Security Button
  V5  - Widget for humidity data (DHT22)
  V6  - Widget for temperature data (DHT22)
  V8  - Widget for show gerkon state
  V9  - Widget for show Pir state
  RTC - real-time clocks
*/

#define BLYNK_MAX_SENDBYTES 256 // max size of bites for email
// Libraries ESP and Blynk
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// Libraries for ESP firmware by air
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Libraries for real-time clocks
#include <TimeLib.h>
#include <WidgetRTC.h>

/*
  #include <DHT.h> // Temperature and Humidity DHT SENSOR
  #define DHTPIN D6
  #define DHTTYPE DHT22
  DHT dht(DHTPIN, DHTTYPE);
*/

#define PIR_PIN D2 // gpio 5 Pir Sensor
#define LED D4
#define GERKON_PIN D5 // door`s gerkon
#define RELAY D6 // D8 = gpio 15
#define BUZZER D7 // D7 = gpio 13

#define BLYNK_GREEN     "#23C48E"
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_DARK_BLUE "#5F7CD8"
#define BLYNK_WHITE     "#FFFFFF"

char auth[] = "*******************";
char ssid[] = "*********";
char pass[] = "*********"; // Set password to "" for open networks.
char Mail[] = "blabla@gmail.com";
String Subject = "Subject: Home Security";

WidgetRTC rtc; // initialize the real-time clock widget
BlynkTimer timer; // initialize timer
int timerPIRSensor_ID; // initialize the variable to store the motion timer timer ID
int timerMain_Door_ID;
int timerbuzzer_ID;

WidgetLED led1(V1); // register widget LED

bool isFirstConnect = true; // Variable to store the state for the first time
bool pirState = false;    // initialize the variable to store the status of the alarm triggered by the motion sensor
bool gerkonState = false; // initialize the variable to store the status of the alarm triggered by the gerkon
bool flagProtection = false;
bool intruderState = LOW;  // The variable that stores the signaling alarm
int securityStateX = 0;    // Alarm status number

///////////////////////////// MAIN CODE ///////////////////////////

BLYNK_CONNECTED() { // If the connection was established for the first time, then we synchronize all the widgets
  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String currentDate = String(day()) + "/" + month() + "/" + year() + " ";
  if (isFirstConnect) {
    Blynk.syncAll(); // Synchronize all widgets
    String Start_Notif = "The alarm system is started " + currentDate + " " + currentTime;
    Blynk.notify(Start_Notif);
    //Blynk.email(Mail , "Subject: Security ESP ", "The alarm system is started");
    isFirstConnect = false;
  }
}

void readPIRSensor() { // function for PIR Sensor
  static int pir = LOW; // variable for storing the status of the motion sensor
  if (flagProtection == true && pirState == LOW) { // If security is enabled
    pir  = digitalRead(PIR_PIN); // read the value from the digital input
  }
  if (flagProtection == true && pir == HIGH && pirState == LOW) { // If the security is ON and the Motion Sensor is activated
    pirState = HIGH; // This flag can be reset only by disabling the alarm
  }
  if (flagProtection == false && pirState == HIGH) { // If the security is OFF
    pirState = LOW;
  }
}

void door_gerkon() {
  static int gerkon = HIGH; // variable for storing the status of the Gerkon
  if (flagProtection == true && gerkonState == LOW) { // if security enable
    gerkon  = digitalRead(GERKON_PIN);
    Serial.println("Gerkon" + String(gerkon));
  }
  if (flagProtection == true && gerkon == LOW && gerkonState == LOW) { // If the security is ON and the Gerkon is activated
    gerkonState = HIGH;
  }
  if (flagProtection == false && gerkonState == HIGH) {
    gerkonState = LOW;
  }
}

BLYNK_WRITE(V2) { // Read the status of the Security button and save the value in flagProtection
  flagProtection = param.asInt(); // Read the status of the security button
  if (flagProtection) {
    timer.enable(timerMain_Door_ID);
    timer.enable(timerPIRSensor_ID); // Enable the timer for the readPIRSensor function
  }
  else {
    timer.disable(timerPIRSensor_ID); // Disable the timer for the readPIRSensor function
    timer.setTimeout(50, readPIRSensor);
    timer.disable(timerMain_Door_ID);
    timer.setTimeout(100, door_gerkon);
    Blynk.virtualWrite(V8, " "); // clear the status line
    Blynk.virtualWrite(V9, " ");
  }
}

void message() { // function for sending messages, depending on the status of the Alarm (securityState)
  String Notif; // Variable to store the message
  String currentDate = String(day()) + "/" + month() + "/" + year() + " "; // RTC date
  String currentTime = String(hour()) + ":" + minute() + ":" + second(); // RTC time
  switch (securityStateX) {
    case 1:
      Notif = "Motion Detected  " + currentDate + " " + currentTime;
      break;
    case 5:
      Notif = "Attention, was activated motion sensor " + currentDate + " " + currentTime;
      break;
    case 6:
      Notif = "Attention, door was opened " + currentDate + " " + currentTime;
      break;
  }
  static int i = 1;
  if (i == 2) {
    Blynk.notify(Notif); i = 1;
  }
  else if (i == 1) {
    Blynk.email(Mail, Subject, Notif); i++;
  }

  //Blynk.notify(Notif);
  //Blynk.email(Mail, Subject, Notif);
}

bool oneFlag = false;
void securityState () { // Define the status of the Alarm
  // State 1 - Motion detected"
  if (flagProtection == true && pirState == true && gerkonState == true && securityStateX != 1) {
    securityStateX = 1;
    intruderState = HIGH; // the alarm worked
    timer.setTimer(1000, message, 2); // call the function message 2 times, with an interval of 1 sec
    Blynk.setProperty(V10, "color", BLYNK_RED);
    Blynk.setProperty(V2, "color", BLYNK_RED);
    Blynk.virtualWrite(V10, "Motion Detected");
  }
  // State 2 - Security ON
  else if (flagProtection == true && pirState == false && securityStateX != 2) {
    securityStateX = 2;
    timer.setTimer(1000, message, 2);
    Blynk.setProperty(V10, "color", BLYNK_GREEN);
    Blynk.setProperty(V2, "color", BLYNK_GREEN);
    Blynk.virtualWrite(V10, "Security ON");
  }
  // State 3 - Security OFF
  else if (flagProtection == false && securityStateX != 3) {
    securityStateX = 3;
    intruderState = LOW;
    timer.setTimer(1000, message, 2);
    Blynk.setProperty(V10, "color", BLYNK_BLUE);
    Blynk.setProperty(V2, "color", BLYNK_BLUE);
    Blynk.virtualWrite(V10, "Security OFF");
    oneFlag = false;
  }
  // State 5 - Attention, was activated motion sensor
  else if (flagProtection == true && pirState == true && securityStateX != 5) {
    securityStateX = 5;
    timer.setTimer(1000, message, 2);
    Serial.println("Pir atention securityStateX " + String(securityStateX));
    Blynk.virtualWrite(V9, "Attention, was activated motion sensor");
  }
  // State 6 - Attention, door was opened
  else if (flagProtection == true && gerkonState == true && securityStateX != 6 && securityStateX != 1) {
    securityStateX = 6;
    Serial.println("oneflag: " + String(oneFlag));
    Serial.println("securityStateX " + String(securityStateX));
    timer.setTimer(1000, message, 2);
    Blynk.virtualWrite(V8, "Attention, door was opened");
    timer
  }
  if (securityStateX  == 6){Serial.println("all good");}
}

void buzzer() {
  tone(BUZZER, 3500, 50); // Set the frequency and duration for Buzzer
}

/*
  void temp() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  Blynk.virtualWrite(V6, t);
  static int hotState = 0;
  if (t > 30 && hotState != 1) {
    hotState = 1;
    Blynk.setProperty(V5, "label", "Critical Temp");
    Blynk.setProperty(V5, "color", BLYNK_RED);
  }
  else {
    Blynk.virtualWrite(V5, h);
  }
  }
*/

void reconnectBlynk() { // check connection
  if (!Blynk.connected()) { //if no connection
    if (Blynk.connect()) {
      BLYNK_LOG("Reconnected");
    }
    else {
      BLYNK_LOG("Not reconnected");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode (LED, OUTPUT);
  digitalWrite(LED, HIGH);
  pinMode (PIR_PIN, INPUT);
  pinMode (GERKON_PIN, INPUT);
  pinMode (BUZZER, OUTPUT);
  delay(1000);
  for (int i = 0; i <= 3; i++) {
    tone(BUZZER, 300, 50);
    delay(80);
  }
  // Blynk.begin(auth, ssid, pass); // because of it hangs ESP8266 when trying to establish a WiFi connection
  Blynk.config(auth); // configure the connection
  Blynk.disconnect(); // break connection
  Blynk.connect();  // will connect

  // Set up timers
  timerPIRSensor_ID = timer.setInterval(1000L, readPIRSensor); // call Pir sensor function 1 time of 1 second
  timerMain_Door_ID = timer.setInterval(1500L, door_gerkon);
  timerbuzzer_ID = timer.setInterval(500L, buzzer); // Call function once after 500 milliseconds.
  timer.disable(timerPIRSensor_ID); // Disable Motion Sensor Timer
  timer.disable(timerMain_Door_ID);
  timer.disable(timerbuzzer_ID);  // Disable Motion Sensor Timer
  timer.setInterval(3000L, securityState); // call func State of security 1 time of 3 seconds
  timer.setInterval(30000L, reconnectBlynk);  // check every 50 seconds if it's still connected to the server
  //timer.setInterval(2000L, temp); // call func of Dht sensor
  rtc.begin(); // start the real-time clock widget
  led1.on();
  Blynk.setProperty(V10, "label", "Security state"); // Set the name of the widget
  Blynk.setProperty(V2, "label", "Security");


  //************************
  ArduinoOTA.setHostname("Home-security"); // Specify the name of the OTA network port
  ArduinoOTA.setPassword((const char *)"1111"); // Set access password for remote OTA firmware
  ArduinoOTA.begin(); // Initializing OTA
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //***********************


  if (Blynk.connected()) {  // If a connection is established, then turn on the piezo twice
    for (int i = 0; i <= 2; i++) {
      tone(BUZZER, 200, 50);
      delay(100);
    }
  }
  else {
    for (int i = 0; i <= 10; i++) {
      tone(BUZZER, 100, 50);
      delay(80);
    }
  }
}

void loop() {
  ArduinoOTA.handle(); // OTA Always ready for air flashingwires
  if (Blynk.connected()) {
    Blynk.run();
  }
  timer.run();
}
