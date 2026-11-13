#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Adafruit_BMP085.h>
#include <U8g2lib.h> // Nutzt U8g2 für saubere 64x48 Ansteuerung
#include <time.h>

// WLAN Zugangsdaten
const char* ssid = "WLAN-SSID";
const char* password = "WLAN_Password";

// Webserver auf Port 80
WebServer server(80);

// U8g2 Display-Konfiguration für 0.66" 64x48 I2C OLED (Nutzt die I2C Pins 8 & 10)
U8G2_SSD1306_64X48_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// BMP180 Sensor
Adafruit_BMP085 bmp;

// Globale Variablen
float currentTemp = 0.0;
float currentPressure = 0.0;
String timeString = "00:00:00";
String apiStatus = "API: Aktiv";
unsigned long lastApiCall = 0;
const long apiInterval = 60000; // API alle 60 Sekunden abfragen

// NTP Zeitserver Konfiguration für Deutschland (MEZ/MESZ)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// Echte HTTP-API abrufen (erfüllt die API-Vorgabe für Level 3)
void fetchAPI() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://ip-api.com/json/");
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      apiStatus = "API: Online (OK)";
    } else {
      apiStatus = "API: Fehler " + String(httpResponseCode);
    }
    http.end();
  } else {
    apiStatus = "API: Kein WLAN";
  }
}

// Aktuelle Uhrzeit vom NTP-Server holen
void updateTime() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeBuffer[10];
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
    timeString = String(timeBuffer);
  }
}

// Webserver-Ansicht im LAN
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv=\"refresh\" content=\"2\">";
  html += "<meta charset=\"UTF-8\">";
  html += "<title>ESP32-C3 Wetterstation</title>";
  html += "<style>body{font-family:Arial; text-align:center; margin-top:20px; background:#f4f4f4;}";
  html += ".card{background:white; padding:15px; border-radius:10px; display:inline-block; box-shadow:0px 0px 10px rgba(0,0,0,0.1); margin:10px; width:260px;}";
  html += "h1{color:#333; font-size:16px;} p{font-size:15px; margin:8px 0;}</style>";
  html += "</head><body>";
  
  html += "<div class=\"card\">";
  html += "<h1>Lokaler Sensor (BMP180)</h1>";
  html += "<p><b>Temperatur:</b> " + String(currentTemp, 1) + " &deg;C</p>";
  html += "<p><b>Luftdruck:</b> " + String(currentPressure, 1) + " hPa</p>";
  html += "</div>";

  html += "<div class=\"card\">";
  html += "<h1>Zeit & API Status</h1>";
  html += "<p><b>Uhrzeit:</b> " + timeString + "</p>";
  html += "<p><b>Status:</b> " + apiStatus + "</p>";
  html += "</div>";

  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // I2C Initialisierung für ESP32-C3 (SDA = 8, SCL = 10)
  Wire.begin(8, 10);

  // U8g2 Display starten
  u8g2.begin();

  // BMP180 Sensor starten
  if (!bmp.begin()) {
    Serial.println("Konnte BMP180 nicht finden!");
    while (1);
  }

  // WLAN Verbindung herstellen
  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Verbunden! IP-Adresse: ");
  Serial.println(WiFi.localIP());

  // NTP Zeit-Client starten
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Ersten API-Abruf starten
  fetchAPI();

  // Webserver starten
  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();

  // Sensordaten lokal auslesen
  currentTemp = bmp.readTemperature();
  currentPressure = bmp.readPressure() / 100.0F;

  // Uhrzeit aktualisieren
  updateTime();

  // API periodisch im Hintergrund abfragen
  if (millis() - lastApiCall > apiInterval || lastApiCall == 0) {
    fetchAPI();
    lastApiCall = millis();
  }

  // --- OLED Display Zeichnen mit U8g2 ---
  u8g2.firstPage();
  do {
    // Gut lesbare Standardschrift wählen
    u8g2.setFont(u8g2_font_helvB08_tf);
    
    // Zeile 1: Temperatur
    char tempBuf[16];
    sprintf(tempBuf, "T: %.1f C", currentTemp);
    u8g2.drawStr(0, 10, tempBuf);

    // Zeile 2: Luftdruck
    char pressBuf[16];
    sprintf(pressBuf, "P: %d hPa", (int)currentPressure);
    u8g2.drawStr(0, 26, pressBuf);

    // Zeile 3: Uhrzeit
    u8g2.drawStr(0, 42, timeString.c_str());

  } while (u8g2.nextPage());

  delay(1000);
}