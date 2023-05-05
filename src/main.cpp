#include <Arduino.h>
#include "DHT20.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <Servo.h>
#include <string>
#include <cstring>
#include "time.h"
// This example downloads the URL "http://arduino.cc/"

//Web section
char ssid[] = "mix_hotspot"; // your network SSID (name)
char pass[] = "33233235512"; // your network password (use for WPA, or use as key for WEP)
// Name of the server we want to connect to
const char kHostname[] = "54.176.176.121";
// Path to download (this is the bit after the hostname in the URL
// that you want to download
const int kPort = 5000;

char *kPath; //= "/api/timezone/Europe/London.txt";
// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30 * 1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;
char lastMinute;
char timeMinute[3];
struct tm timeinfo;

// TDS section
#define TdsSensorPin 33
#define BUTTON1PIN 35
#define BUTTON2PIN 0
volatile bool ledState = false;
#define LED1PIN 25
#define HEATERPIN 12
#define SERVOPIN = 27
#define VREF 1.1          // analog reference voltage(Volt) of the ADC
#define SCOUNT 30         // sum of sample point
int analogBuffer[SCOUNT]; // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0;
float averageVoltage = 0, tdsValue = 0, temperature = 25;

static unsigned long analogSampleTimepoint = millis();
static unsigned long tempTimer = millis();
static unsigned long printTimer = millis();
// DHT section
DHT20 DHT;
uint8_t count = 0;
float humidity;
float temp;
std::string tempStr = "/?temp=";
std::string humidityStr = "&humidity=";
std::string tdsStr = "&tds=";
std::string params;

// LCD section
TFT_eSPI tft = TFT_eSPI();           // Create object "tft"
TFT_eSprite img = TFT_eSprite(&tft); // Create Sprite object "img" with pointer to "tft" object
//                                    // the pointer is used by pushSprite() to push it onto the TFT


//Servo section
Servo myservo;

//photo resistor

#define PHOTO_RESISTOR_PIN 36


int getMedianNum(int bArray[], int iFilterLen)
{
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++)
  {
    for (i = 0; i < iFilterLen - j - 1; i++)
    {
      if (bTab[i] > bTab[i + 1])
      {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
  else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  return bTemp;
}


void readParam()
{

  // READ DATA
  uint32_t start = micros();
  int status = DHT.read();
  uint32_t stop = micros();

  if ((count % 10) == 0)
  {
    count = 0;
  }
  count++;
  Serial.println();
  Serial.println();
  Serial.println("Type\tHumidity (%)\tTemp (°C)\tTime (µs)\tStatus");
  tempStr = "/?temp=" + std::to_string(temp);
  humidityStr = "&humidity=" + std::to_string(humidity);
  tdsStr = "&tds=" + std::to_string(tdsValue);

  params = tempStr + tdsStr;
  kPath = new char[params.length() + 1];
  strcpy(kPath, params.c_str());
  // Serial.println("kpath is:");
  // Serial.println(kPath);

  Serial.print("DHT20 \t");
  // DISPLAY DATA, sensor has only one decimal.
  humidity = DHT.getHumidity();
  temp = DHT.getTemperature();
  Serial.print(humidity, 1);
  Serial.print("\t\t");
  Serial.print(temp, 1);
  Serial.print("\t\t");
  Serial.print(stop - start);
  Serial.print("\t\t");
  switch (status)
  {
  case DHT20_OK:
    Serial.print("OK");
    break;
  case DHT20_ERROR_CHECKSUM:
    Serial.print("Checksum error");
    break;
  case DHT20_ERROR_CONNECT:
    Serial.print("Connect error");
    break;
  case DHT20_MISSING_BYTES:
    Serial.print("Missing bytes");
    break;
  case DHT20_ERROR_BYTES_ALL_ZERO:
    Serial.print("All bytes read zero");
    break;
  case DHT20_ERROR_READ_TIMEOUT:
    Serial.print("Read time out");
    break;
  case DHT20_ERROR_LASTREAD:
    Serial.print("Error read too fast");
    break;
  default:
    Serial.print("Unknown error");
    break;
  }
  Serial.print("\n");
  Serial.print("\n");
}


void IRAM_ATTR toggleButton1() {
  //Serial.println("Button 1 Pressed!");
  //ledState = !ledState;
  digitalWrite(LED1PIN, HIGH);
  
}

void IRAM_ATTR toggleButton2() {
  //Serial.println("Button 2 Pressed!");
  digitalWrite(LED1PIN, LOW);
}

void releaseFood() {
  myservo.write(0);
  delay(1000);
  myservo.write(180);
}

void makeRequest()
{
  int err = 0;

  WiFiClient c;
  HttpClient http(c);

  err = http.get(kHostname, 5000, kPath);
  if (err == 0)
  {
    Serial.println("startedRequest ok");

    err = http.responseStatusCode();
    if (err >= 0)
    {
      Serial.print("Got status code: ");
      Serial.println(err);

      // Usually you'd check that the response code is 200 or a
      // similar "success" code (200-299) before carrying on,
      // but we'll print out whatever response we get

      err = http.skipResponseHeaders();
      if (err >= 0)
      {
        int bodyLen = http.contentLength();
        Serial.print("Content length is: ");
        Serial.println(bodyLen);
        Serial.println();
        Serial.println("Body returned follows:");

        // Now we've got to the body, so we can print it out
        unsigned long timeoutStart = millis();
        char c;
        // Whilst we haven't timed out & haven't reached the end of the body
        while ((http.connected() || http.available()) &&
               ((millis() - timeoutStart) < kNetworkTimeout))
        {
          if (http.available())
          {
            c = http.read();
            // Print out this character
            Serial.print(c);

            bodyLen--;
            // We read something, reset the timeout counter
            timeoutStart = millis();
          }
          else
          {
            // We haven't got any data, so let's pause to allow some to
            // arrive
            delay(kNetworkDelay);
          }
        }
      }
      else
      {
        Serial.print("Failed to skip response headers: ");
        Serial.println(err);
      }
    }
    else
    {
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  }
  else
  {
    Serial.print("Connect failed: ");
    Serial.println(err);
  }
  http.stop();
}

void connectionInit()
{
  // We start by connecting to a WiFi network
  delay(1000);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());

  makeRequest();
}

void printLocalTime(){
  
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  
  strftime(timeHour,3, "%H", &timeinfo);
  strftime(timeMinute,3, "%M", &timeinfo);

  lastMinute=timeMinute[1];

  Serial.println(timeHour);
  Serial.println(timeMinute);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
}

void updateMinute() {
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  strftime(timeMinute,3, "%M", &timeinfo);
}

void setup()
{
  // put your setup code here, to run once:
  DHT.begin();
  Serial.begin(9600);

  myservo.attach(27); // attaches the servo on port 27 to the servo object
  releaseFood();

  Serial.println(__FILE__);
  Serial.print("DHT20 LIBRARY VERSION: ");
  Serial.println(DHT20_LIB_VERSION);
  Serial.println();
  pinMode(TdsSensorPin, INPUT);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  pinMode(LED1PIN, OUTPUT);
  pinMode(HEATERPIN, OUTPUT);
  pinMode(BUTTON1PIN, INPUT_PULLUP);
  pinMode(BUTTON2PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON1PIN), toggleButton1, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON2PIN), toggleButton2, FALLING);


  connectionInit();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  
}

void loop()
{

  if (millis() - tempTimer > 1000U)
  {
    readParam();


    updateMinute();
    if (lastMinute != timeMinute[1])
    {
      lastMinute = timeMinute[1];
      releaseFood();
    }
    int lightVal = analogRead(PHOTO_RESISTOR_PIN);
    if (lightVal < 1000) {
      digitalWrite(LED1PIN, HIGH);
    }
    else if(lightVal > 1000) {
      digitalWrite(LED1PIN, LOW);
    }
    tempTimer = millis();
  }
  if (millis() - printTimer > 1000U)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 4);
    // tft.setTextFont(4);
    tft.print("Temp: ");
    tft.println(temp);
    tft.print("TDS: ");
    tft.println(tdsValue);
    printTimer = millis();

    if (temp < 25.0f) {
      digitalWrite(HEATERPIN, HIGH);
    }

    else if (temp > 26.0f)
    {
      digitalWrite(HEATERPIN, LOW);
    }

    if(tdsValue > 200) {
      tft.setTextColor(TFT_RED);
      tft.println("Filtration system ON");
      tft.setTextColor(TFT_WHITE);
    }
    makeRequest();
  }

  static unsigned long analogSampleTimepoint = millis();
   if(millis()-analogSampleTimepoint > 40U)     //every 40 milliseconds,read the analog value from the ADC
   {
     analogSampleTimepoint = millis();
     analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
     analogBufferIndex++;
     if(analogBufferIndex == SCOUNT) 
         analogBufferIndex = 0;
   }   
   static unsigned long printTimepoint = millis();
   if(millis()-printTimepoint > 800U)
   {
      printTimepoint = millis();
      for(copyIndex=0;copyIndex<SCOUNT;copyIndex++)
        analogBufferTemp[copyIndex]= analogBuffer[copyIndex];
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 1024.0; // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      float compensationCoefficient=1.0+0.02*(temperature-25.0);    //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
      float compensationVolatge=averageVoltage/compensationCoefficient;  //temperature compensation
      tdsValue=(133.42*compensationVolatge*compensationVolatge*compensationVolatge - 255.86*compensationVolatge*compensationVolatge + 857.39*compensationVolatge)*0.5; //convert voltage value to tds value
      Serial.print("TDS Value:");
      Serial.print(tdsValue,0);
      Serial.println("ppm");
      Serial.println(timeMinute);
   }
  
}
