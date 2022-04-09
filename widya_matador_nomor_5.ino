#include "configuration/config.h"             // import config
#include <ArduinoJson.h>                      // ArduinoJson
#include "SparkFun_I2C_GPS_Arduino_Library.h" //Use Library Manager or download here: https://github.com/sparkfun/SparkFun_I2C_GPS_Arduino_Library
I2CGPS myI2CGPS;                              // Hook object to the library

#define PORT Serial1

// sim800

#define SerialMon Serial
#define TINY_GSM_MODEM_SIM800
#define SerialAT Serial2
#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif

#define TINY_GSM_DEBUG SerialMon
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200

#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// set GSM PIN, if any
#define GSM_PIN ""

const char apn[] = "indosat";
const char gprsUser[] = "indosat";
const char gprsPass[] = "indosat";

// Your WiFi connection credentials, if applicable
const char wifiSSID[] = "YourSSID";
const char wifiPass[] = "YourWiFiPass";

// link untuk read http
const char server[] = "vsh.pp.ua";
const char resource[] = "/TinyGSM/logo.txt";
const int port = 80;

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

// Just in case someone defined the wrong thing..
#if TINY_GSM_USE_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true
#endif
#if TINY_GSM_USE_WIFI && not defined TINY_GSM_MODEM_HAS_WIFI
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#endif

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

TinyGsmClient client(modem);
HttpClient http(client, server, port);

// sim800

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

// define two tasks for Blink & AnalogRead
void TaskBlink(void *pvParameters);
void TaskAnalogRead(void *pvParameters);

// setup arduino ArduinoJson

StaticJsonBuffer<1000> jsonBuffer;
JsonObject &root = jsonBuffer.createObject();

// setup arduino ArduinoJson

// setup grps sim

// setup grps sim

void setupSIM()
{
  SerialMon.println("Wait...");

  // Set GSM module baud rate
  TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  // SerialAT.begin(9600);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // modem.init();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

#if TINY_GSM_USE_GPRS
  // Unlock your SIM card with a PIN if needed
  if (GSM_PIN && modem.getSimStatus() != 3)
  {
    modem.simUnlock(GSM_PIN);
  }
#endif
}

void setupGPSIIC()
{
  PORT.begin(115200);
  PORT.println("GTOP Read Example");

  while (myI2CGPS.begin() == false)
  {
    PORT.println("Module failed to respond. Please check wiring.");
    delay(500);
  }
  PORT.println("GPS module found!");
}

// the setup function runs once when you press reset or power the board
void setup()
{

  // initialize serial communication at 115200 bits per second:
  SerialMon.begin(baudrate);
  // Serial1.begin(baudrateGPRS, PROTOCOL, RXD1, TXD1);

  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(
      TaskBlink, "TaskBlink" // A name just for humans
      ,
      1024 // This stack size can be checked & adjusted by reading the Stack Highwater
      ,
      NULL, 2 // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
      ,
      NULL, ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
      TaskAnalogRead, "AnalogReadA3", 1024 // Stack size
      ,
      NULL, 1 // Priority
      ,
      NULL, ARDUINO_RUNNING_CORE);

  setupSIM();

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}

void loopGPRS()
{
#if TINY_GSM_USE_WIFI
  // Wifi connection parameters must be set before waiting for the network
  SerialMon.print(F("Setting SSID/password..."));
  if (!modem.networkConnect(wifiSSID, wifiPass))
  {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");
#endif

#if TINY_GSM_USE_GPRS && defined TINY_GSM_MODEM_XBEE
  // The XBee must run the gprsConnect function BEFORE waiting for network!
  modem.gprsConnect(apn, gprsUser, gprsPass);
#endif

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork())
  {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected())
  {
    SerialMon.println("Network connected");
  }

#if TINY_GSM_USE_GPRS
  // GPRS connection parameters are usually set after network registration
  SerialMon.print(F("Connecting to "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass))
  {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");

  if (modem.isGprsConnected())
  {
    SerialMon.println("GPRS connected");
  }
#endif

  SerialMon.print(F("Performing HTTP GET request... "));
  int err = http.get(resource);
  if (err != 0)
  {
    SerialMon.println(F("failed to connect"));
    delay(10000);
    return;
  }

  int status = http.responseStatusCode();
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status)
  {
    delay(10000);
    return;
  }

  SerialMon.println(F("Response Headers:"));
  while (http.headerAvailable())
  {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    SerialMon.println("    " + headerName + " : " + headerValue);
  }

  int length = http.contentLength();
  if (length >= 0)
  {
    SerialMon.print(F("Content length is: "));
    SerialMon.println(length);
  }
  if (http.isResponseChunked())
  {
    SerialMon.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);

  SerialMon.print(F("Body length is: "));
  SerialMon.println(body.length());

  // Shutdown

  http.stop();
  SerialMon.println(F("Server disconnected"));

#if TINY_GSM_USE_WIFI
  modem.networkDisconnect();
  SerialMon.println(F("WiFi disconnected"));
#endif
#if TINY_GSM_USE_GPRS
  modem.gprsDisconnect();
  SerialMon.println(F("GPRS disconnected"));
#endif

  // Do nothing forevermore
  while (true)
  {
    delay(1000);
  }
}

void loopGPS()
{
  while (myI2CGPS.available()) // available() returns the number of new bytes available from the GPS module
  {
    byte incoming = myI2CGPS.read(); // Read the latest byte from Qwiic GPS

    if (incoming == '$')
      PORT.println();     // Break the sentences onto new lines
    PORT.write(incoming); // Print this character
  }
}

void loop()
{
  loopGPRS(); // ini async, jadi di letakkan pada loop lanhsunh tidak pada rtos
  loopGPS();  // ini async, jadi di letakkan pada loop lanhsunh tidak pada rtos
  
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskBlink(void *pvParameters) // This is a task.
{
  (void)pvParameters;

  // initialize digital LED_BUILTIN on pin 13 as an output.
  pinMode(led, OUTPUT); // internal led esp32 is 2

  for (;;) // A Task shall never return or exit.
  {
    digitalWrite(led, HIGH);
    vTaskDelay(1000);
    digitalWrite(led, LOW);
    vTaskDelay(1000);
  }
}

void TaskAnalogRead(void *pvParameters) // This is a task.
{
  (void)pvParameters;

  for (;;)
  {
    // read the input on analog pin 36:
    int sensorValue = analogRead(analog);

    // dump data to json format
    root["ADC Value"] = sensorValue;

    root.printTo(Serial); // cetak json

    int totalData = 1000 / analogData;

    vTaskDelay(totalData); // total pengambilan 100 data, jadi Vdelay  = 10
  }
}