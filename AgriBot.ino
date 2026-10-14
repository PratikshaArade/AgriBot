#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>

//================ WiFi AP ==================
const char* AP_SSID = "AGRIBOT_PWM";
const char* AP_PASS = "12345678";

WebServer server(80);

//================ LCD ==================
LiquidCrystal_I2C lcd(0x27, 20, 4);

//================ DHT ==================
#define DHTPIN 18
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//============= Sensors =================
#define LDR_PIN     39
#define RAIN_PIN    35
#define FLOAT_PIN   32
#define SOIL_PIN    13

//=========== L293D Driver ==============
#define ENB 2      // Motor 2 Enable - LED Strip
#define ENA 5      // Motor 1 Enable - Pump

#define IN1 17     // Motor 1 input 1
#define IN2 16     // Motor 1 input 2

#define IN3 4      // Motor 2 input 1
#define IN4 15     // Motor 2 input 2

// PWM values
int pumpPWM = 255;             // Default 25%
const int LED_SPEED = 255;    // 100%

// Motor direction
// 1  = Forward
// -1 = Reverse
int motorDirection = 1;

unsigned long previousLCD = 0;
unsigned long previousDHT = 0;

float temperature = 0;
float humidity = 0;

bool pumpStatus = false;
bool ledStatus = false;
bool tankEmpty = false;
bool soilDry = false;
bool noRain = false;
bool dark = false;

//================ LCD Helper ==================
void printLine(byte row, String text)
{
  lcd.setCursor(0, row);

  if (text.length() > 20)
  {
    text = text.substring(0, 20);
  }

  while (text.length() < 20)
  {
    text += " ";
  }

  lcd.print(text);
}

//================ Output Functions ==================
void pumpOff()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  ledcWrite(ENA, 0);
}

void pumpOn()
{
  if (motorDirection == 1)
  {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  }
  else
  {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  }

  ledcWrite(ENA, pumpPWM);
}

void ledStripOff()
{
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  ledcWrite(ENB, 0);
}

void ledStripOn()
{
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  ledcWrite(ENB, LED_SPEED);
}

//================ Web Page ==================
String webpage()
{
  int percent = map(pumpPWM, 0, 255, 0, 100);
  String dirText = motorDirection == 1 ? "FORWARD" : "REVERSE";

  String html = "";
  html += "<!DOCTYPE html><html>";
  html += "<head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Agribot PWM</title>";

  html += "<style>";
  html += "body{font-family:Arial;background:#eef7ee;text-align:center;padding:20px;}";
  html += ".card{background:white;border-radius:15px;padding:20px;max-width:440px;margin:auto;box-shadow:0 0 12px #aaa;}";
  html += "h2{color:#1b5e20;}";
  html += "input[type=range]{width:90%;}";
  html += ".value{font-size:28px;font-weight:bold;color:#0d47a1;}";
  html += ".status{font-size:17px;margin-top:10px;line-height:1.6;}";
  html += "button{padding:10px 18px;border:0;border-radius:8px;margin:5px;color:white;font-size:16px;}";
  html += ".fwd{background:#1b5e20;}";
  html += ".rev{background:#b71c1c;}";
  html += ".def{background:#0d47a1;}";
  html += "</style>";

  html += "</head>";
  html += "<body>";

  html += "<div class='card'>";
  html += "<h2>SMART AGRIBOT</h2>";

  html += "<h3>Motor 1 Pump PWM</h3>";

  html += "<div class='value'><span id='pwmVal'>";
  html += String(pumpPWM);
  html += "</span> / 255</div>";

  html += "<div><span id='percentVal'>";
  html += String(percent);
  html += "</span>%</div><br>";

  html += "<input type='range' min='0' max='255' value='";
  html += String(pumpPWM);
  html += "' id='pwmSlider' oninput='updatePWM(this.value)'>";

  html += "<br><br>";
  html += "<button class='def' onclick='setDefault()'>Set 25%</button>";

  html += "<h3>Motor Direction</h3>";
  html += "<div class='value' id='dirVal'>";
  html += dirText;
  html += "</div>";

  html += "<button class='fwd' onclick='setDirection(1)'>Forward</button>";
  html += "<button class='rev' onclick='setDirection(-1)'>Reverse</button>";

  html += "<div class='status'>";
  html += "Pump: <span id='pump'>--</span><br>";
  html += "PWM: <span id='pwmStatus'>--</span><br>";
  html += "Direction: <span id='direction'>--</span><br>";
  html += "Tank: <span id='tank'>--</span><br>";
  html += "Soil: <span id='soil'>--</span><br>";
  html += "Rain: <span id='rain'>--</span><br>";
  html += "Light: <span id='light'>--</span>";
  html += "</div>";

  html += "</div>";

  html += "<script>";

  html += "function updatePWM(val){";
  html += "document.getElementById('pwmVal').innerHTML=val;";
  html += "document.getElementById('percentVal').innerHTML=Math.round((val/255)*100);";
  html += "fetch('/setPWM?value='+val);";
  html += "}";

  html += "function setDefault(){";
  html += "document.getElementById('pwmSlider').value=64;";
  html += "updatePWM(64);";
  html += "}";

  html += "function setDirection(dir){";
  html += "fetch('/setDIR?value='+dir).then(()=>refreshStatus());";
  html += "document.getElementById('dirVal').innerHTML=(dir==1?'FORWARD':'REVERSE');";
  html += "}";

  html += "function refreshStatus(){";
  html += "fetch('/status').then(r=>r.json()).then(data=>{";
  html += "document.getElementById('pump').innerHTML=data.pump;";
  html += "document.getElementById('pwmStatus').innerHTML=data.pwm;";
  html += "document.getElementById('direction').innerHTML=data.direction;";
  html += "document.getElementById('dirVal').innerHTML=data.direction;";
  html += "document.getElementById('tank').innerHTML=data.tank;";
  html += "document.getElementById('soil').innerHTML=data.soil;";
  html += "document.getElementById('rain').innerHTML=data.rain;";
  html += "document.getElementById('light').innerHTML=data.light;";
  html += "});";
  html += "}";

  html += "setInterval(refreshStatus,1000);";
  html += "refreshStatus();";

  html += "</script>";
  html += "</body></html>";

  return html;
}

//================ Web Handlers ==================
void handleRoot()
{
  server.send(200, "text/html", webpage());
}

void handleSetPWM()
{
  if (server.hasArg("value"))
  {
    int value = server.arg("value").toInt();

    if (value < 0) value = 0;
    if (value > 255) value = 255;

    pumpPWM = value;

    if (pumpStatus)
    {
      ledcWrite(ENA, pumpPWM);
    }

    Serial.print("New Pump PWM: ");
    Serial.println(pumpPWM);

    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(400, "text/plain", "Missing value");
  }
}

void handleSetDIR()
{
  if (server.hasArg("value"))
  {
    int value = server.arg("value").toInt();

    if (value == 1)
    {
      motorDirection = 1;
    }
    else if (value == -1)
    {
      motorDirection = -1;
    }

    if (pumpStatus)
    {
      pumpOn();
    }

    Serial.print("New Motor Direction: ");
    Serial.println(motorDirection == 1 ? "FORWARD" : "REVERSE");

    server.send(200, "text/plain", "OK");
  }
  else
  {
    server.send(400, "text/plain", "Missing value");
  }
}

void handleStatus()
{
  String json = "{";
  json += "\"pump\":\"" + String(pumpStatus ? "ON" : "OFF") + "\",";
  json += "\"pwm\":\"" + String(pumpPWM) + "\",";
  json += "\"direction\":\"" + String(motorDirection == 1 ? "FORWARD" : "REVERSE") + "\",";
  json += "\"tank\":\"" + String(tankEmpty ? "EMPTY" : "OK") + "\",";
  json += "\"soil\":\"" + String(soilDry ? "DRY" : "WET") + "\",";
  json += "\"rain\":\"" + String(noRain ? "NO" : "YES") + "\",";
  json += "\"light\":\"" + String(dark ? "DARK" : "BRIGHT") + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

//================ Setup ==================
void setup()
{
  Serial.begin(115200);

  pinMode(LDR_PIN, INPUT);
  pinMode(RAIN_PIN, INPUT);
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  pinMode(SOIL_PIN, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // ESP32 Core 3.x PWM API
  ledcAttach(ENA, 1000, 8);
  ledcAttach(ENB, 1000, 8);

  pumpOff();
  ledStripOff();

  lcd.init();
  lcd.backlight();

  dht.begin();

  lcd.clear();
  printLine(0, "SMART AGRIBOT");
  printLine(1, "Starting WiFi AP");
  printLine(2, "SSID: AGRIBOT_PWM");
  printLine(3, "PASS: 12345678");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  IPAddress IP = WiFi.softAPIP();

  Serial.print("AP IP Address: ");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/setPWM", handleSetPWM);
  server.on("/setDIR", handleSetDIR);
  server.on("/status", handleStatus);

  server.begin();

  delay(3000);

  lcd.clear();
  printLine(0, "WiFi AP Active");
  printLine(1, "AGRIBOT_PWM");
  printLine(2, "Open Browser:");
  printLine(3, "192.168.4.1");
  delay(3000);

  lcd.clear();
}

//================ Loop ==================
void loop()
{
  server.handleClient();

  // Read DHT every 2 seconds
  if (millis() - previousDHT >= 2000)
  {
    previousDHT = millis();

    float newTemp = dht.readTemperature();
    float newHum = dht.readHumidity();

    if (!isnan(newTemp))
    {
      temperature = newTemp;
    }

    if (!isnan(newHum))
    {
      humidity = newHum;
    }
  }

  int soil = digitalRead(SOIL_PIN);
  int rain = digitalRead(RAIN_PIN);
  int ldr  = digitalRead(LDR_PIN);

  tankEmpty = !digitalRead(FLOAT_PIN);

  soilDry = soil == HIGH;
  noRain = rain == HIGH;
  dark = ldr == HIGH;

  //================ PUMP CONTROL =================
  if (tankEmpty)
  {
    pumpOff();
    pumpStatus = false;
  }
  else
  {
    if (soilDry && noRain)
    {
      pumpOn();
      pumpStatus = true;
    }
    else
    {
      pumpOff();
      pumpStatus = false;
    }
  }

  //================ LED STRIP CONTROL =================
  if (dark)
  {
    ledStripOn();
    ledStatus = true;
  }
  else
  {
    ledStripOff();
    ledStatus = false;
  }

  //================ SERIAL MONITOR =================
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print(" C | Hum: ");
  Serial.print(humidity);
  Serial.print(" % | Soil: ");
  Serial.print(soilDry ? "DRY" : "WET");
  Serial.print(" | Rain: ");
  Serial.print(noRain ? "NO" : "YES");
  Serial.print(" | Light: ");
  Serial.print(dark ? "DARK" : "BRIGHT");
  Serial.print(" | Tank: ");
  Serial.print(tankEmpty ? "EMPTY" : "OK");
  Serial.print(" | Pump: ");
  Serial.print(pumpStatus ? "ON" : "OFF");
  Serial.print(" | PWM: ");
  Serial.print(pumpPWM);
  Serial.print(" | Dir: ");
  Serial.print(motorDirection == 1 ? "FWD" : "REV");
  Serial.print(" | LED: ");
  Serial.println(ledStatus ? "ON" : "OFF");

  //================ 20x4 LCD DISPLAY =================
  if (millis() - previousLCD >= 1000)
  {
    previousLCD = millis();

    String line0 = "T:" + String(temperature, 1) + (char)223 + "C H:" + String(humidity, 0) + "%";

    String line1 = "Soil:";
    line1 += soilDry ? "DRY " : "WET ";
    line1 += "Rain:";
    line1 += noRain ? "NO" : "YES";

    String line2 = "Tank:";
    line2 += tankEmpty ? "EMPTY " : "OK ";
    line2 += "P:";
    line2 += pumpStatus ? "ON " : "OFF ";
    line2 += motorDirection == 1 ? "FWD" : "REV";

    String line3;

    if (tankEmpty)
    {
      line3 = "!! REFILL WATER !!";
    }
    else
    {
      line3 = "PWM:";
      line3 += String(pumpPWM);
      line3 += " Light:";
      line3 += dark ? "DARK" : "BRIGHT";
    }

    printLine(0, line0);
    printLine(1, line1);
    printLine(2, line2);
    printLine(3, line3);
  }

  delay(100);
}

