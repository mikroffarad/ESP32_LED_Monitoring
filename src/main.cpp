#include <WebServer.h>
#include <HTTPClient.h>
#include <FastLED.h>
#include <Preferences.h>
#include <DNSServer.h>

// Configuration defines
#define LED_PIN 4                    // GPIO pin for LED strip
#define RESET_PIN 14                 // GPIO pin for factory reset button
#define NUM_LEDS 300                 // Number of LEDs in strip
#define INTERNET_CHECK_INTERVAL 5000 // Internet check interval in milliseconds
#define BRIGHTNESS 100               // LED brightness (0-255)

// LED strip
CRGB leds[NUM_LEDS];

// Web server and DNS server
WebServer server(80);
DNSServer dnsServer;

// Preferences for storing settings
Preferences preferences;

// Global variables
String deviceMode = "factory"; // "factory" or "monitoring"
String currentEffect = "waiting";
String staticColor = "#00FF00";
String snakeColor = "#FF0000"; // Default snake color
unsigned long lastInternetCheck = 0;
bool internetStatus = false;
bool factoryResetPressed = false;
unsigned long factoryResetPressTime = 0;

// Effect variables
uint8_t effectHue = 0;
uint8_t breatheBrightness = 50;
int8_t breatheDirection = 1;
unsigned long lastEffectUpdate = 0;
int snakePosition = 0;
int snakeDirection = 1;

// Function declarations
void startFactoryMode();
void startMonitoringMode();
void handleRoot();
void handleMonitoringRoot();
void handleCSS();
void handleWiFiScan();
void handleWiFiConnect();
void handleEffectChange();
void handleFactoryResetWeb();
void handleMonitoringMode();
void handleStatus();
void checkFactoryReset();
void checkInternetConnection();
void updateLEDEffects();
void effectRainbow();
void effectFillRainbow();
void effectStatic();
void effectSnake();
void effectWaiting();
void effectBreatheGreen();
void effectBlinkRed();

void setup()
{
  Serial.begin(115200);
  Serial.println("ESP32 WiFi Monitor Starting...");

  // Initialize LED strip
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  // Initialize preferences
  preferences.begin("wifi-monitor", false);

  // Initialize reset button
  pinMode(RESET_PIN, INPUT_PULLUP);

  // Load saved settings
  String savedSSID = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");

  // Check if we have saved WiFi credentials
  if (savedSSID.length() > 0)
  {
    Serial.println("Attempting to connect to saved WiFi...");
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

    // Wait for connection with timeout
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      deviceMode = "monitoring";
      Serial.println("\nConnected to WiFi!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
    else
    {
      Serial.println("\nFailed to connect to saved WiFi. Starting factory mode.");
      deviceMode = "factory";
    }
  }

  // Start appropriate mode
  if (deviceMode == "factory")
  {
    startFactoryMode();
  }
  else
  {
    startMonitoringMode();
  }

  Serial.println("Setup complete!");
}

void loop()
{
  // Handle DNS server in factory mode
  if (deviceMode == "factory")
  {
    dnsServer.processNextRequest();
  }

  // Handle web server
  server.handleClient();

  // Check factory reset button
  checkFactoryReset();

  // Update LED effects
  updateLEDEffects();

  // Internet monitoring (only in monitoring mode)
  if (deviceMode == "monitoring")
  {
    if (millis() - lastInternetCheck >= INTERNET_CHECK_INTERVAL)
    {
      checkInternetConnection();
      lastInternetCheck = millis();
    }
  }

  delay(10); // Small delay to prevent watchdog reset
}

void startFactoryMode()
{
  Serial.println("Starting Factory Mode (AP)");
  deviceMode = "factory";
  currentEffect = "waiting";

  // Start Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-WiFi-Monitor", "12345678");

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Start DNS server for captive portal
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.println("DNS server started for captive portal");

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/scan", handleWiFiScan);
  server.on("/connect", HTTP_POST, handleWiFiConnect);
  server.on("/effect", HTTP_POST, handleEffectChange);
  server.on("/style.css", handleCSS);
  server.onNotFound(handleRoot); // Redirect all unknown requests to root

  server.begin();
  Serial.println("Factory mode web server started");
}

void startMonitoringMode()
{
  Serial.println("Starting Monitoring Mode");
  deviceMode = "monitoring";
  currentEffect = "monitoring";

  // Setup web server routes for monitoring mode
  server.on("/", handleMonitoringRoot);
  server.on("/effect", HTTP_POST, handleEffectChange);
  server.on("/reset", HTTP_POST, handleFactoryResetWeb);
  server.on("/monitoring", HTTP_POST, handleMonitoringMode);
  server.on("/status", handleStatus);
  server.on("/style.css", handleCSS);

  server.begin();
  Serial.println("Monitoring mode web server started");

  // Initial internet check
  checkInternetConnection();
}

void handleRoot()
{
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32 WiFi Monitor - Setup</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<link rel='stylesheet' href='/style.css'>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP32 WiFi Monitor</h1>";
  html += "<h2>Factory Setup Mode</h2>";

  // WiFi Networks Section
  html += "<div class='section'>";
  html += "<h3>Available WiFi Networks</h3>";
  html += "<button onclick='scanNetworks()' class='btn'>Scan Networks</button>";
  html += "<div id='networks'></div>";
  html += "</div>";

  // Connection Form
  html += "<div class='section'>";
  html += "<h3>Connect to Network</h3>";
  html += "<form onsubmit='connectToWiFi(event)'>";
  html += "<input type='text' id='ssid' placeholder='Network Name (SSID)' required>";
  html += "<input type='password' id='password' placeholder='Password'>";
  html += "<button type='submit' class='btn btn-primary'>Connect</button>";
  html += "</form>";
  html += "</div>";

  // LED Effects Section
  html += "<div class='section'>";
  html += "<h3>LED Effects</h3>";
  html += "<div class='effects-grid'>";
  html += "<button onclick='setEffect(\"rainbow\")' class='btn effect-btn'>Rainbow (HSV)</button>";
  html += "<button onclick='setEffect(\"fill_rainbow\")' class='btn effect-btn'>Rainbow (Fill)</button>";
  html += "<button onclick='setEffect(\"static\")' class='btn effect-btn'>Static Color</button>";
  html += "<button onclick='setEffect(\"snake\")' class='btn effect-btn'>Snake</button>";
  html += "<button onclick='setEffect(\"waiting\")' class='btn effect-btn'>Waiting</button>";
  html += "</div>";
  html += "<div id='colorPicker' style='display:none; margin-top:10px;'>";
  html += "<label>Static Color: </label>";
  html += "<input type='color' id='staticColor' value='#00FF00' onchange='updateStaticColor()'>";
  html += "</div>";
  html += "<div id='snakeColorPicker' style='display:none; margin-top:10px;'>";
  html += "<label>Snake Color: </label>";
  html += "<input type='color' id='snakeColor' value='#FF0000' onchange='updateSnakeColor()'>";
  html += "</div>";
  html += "</div>";

  html += "</div>";
  html += "<script>";
  html += "function scanNetworks() {";
  html += "  fetch('/scan').then(r => r.text()).then(data => {";
  html += "    document.getElementById('networks').innerHTML = data;";
  html += "  });";
  html += "}";
  html += "function selectNetwork(ssid) {";
  html += "  document.getElementById('ssid').value = ssid;";
  html += "}";
  html += "function connectToWiFi(e) {";
  html += "  e.preventDefault();";
  html += "  const ssid = document.getElementById('ssid').value;";
  html += "  const password = document.getElementById('password').value;";
  html += "  fetch('/connect', {";
  html += "    method: 'POST',";
  html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
  html += "    body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`";
  html += "  }).then(r => r.text()).then(data => {";
  html += "    alert(data);";
  html += "    if(data.includes('Success')) setTimeout(() => location.reload(), 3000);";
  html += "  });";
  html += "}";
  html += "function setEffect(effect) {";
  html += "  document.getElementById('colorPicker').style.display = 'none';";
  html += "  document.getElementById('snakeColorPicker').style.display = 'none';";
  html += "  if(effect === 'static') {";
  html += "    document.getElementById('colorPicker').style.display = 'block';";
  html += "  } else if(effect === 'snake') {";
  html += "    document.getElementById('snakeColorPicker').style.display = 'block';";
  html += "  }";
  html += "  fetch('/effect', {";
  html += "    method: 'POST',";
  html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
  html += "    body: `effect=${effect}&color=${document.getElementById('staticColor').value}&snakeColor=${document.getElementById('snakeColor').value}`";
  html += "  });";
  html += "}";
  html += "function updateStaticColor() {";
  html += "  setEffect('static');";
  html += "}";
  html += "function updateSnakeColor() {";
  html += "  setEffect('snake');";
  html += "}";
  html += "scanNetworks();";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleMonitoringRoot()
{
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32 WiFi Monitor - Status</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<link rel='stylesheet' href='/style.css'>";
  html += "<meta http-equiv='refresh' content='10'>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP32 WiFi Monitor</h1>";
  html += "<h2>Monitoring Mode</h2>";

  // Status Section
  html += "<div class='section'>";
  html += "<h3>Connection Status</h3>";
  html += "<div class='status-grid'>";
  html += "<div class='status-item'>";
  html += "<span class='label'>WiFi:</span>";
  html += "<span class='value " + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "'>";
  html += WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected";
  html += "</span></div>";
  html += "<div class='status-item'>";
  html += "<span class='label'>Network:</span>";
  html += "<span class='value'>" + WiFi.SSID() + "</span>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<span class='label'>IP Address:</span>";
  html += "<span class='value'>" + WiFi.localIP().toString() + "</span>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<span class='label'>Internet:</span>";
  html += "<span class='value " + String(internetStatus ? "connected" : "disconnected") + "'>";
  html += internetStatus ? "Available" : "Not Available";
  html += "</span></div>";
  html += "</div>";
  html += "</div>";

  // LED Effects Section
  html += "<div class='section'>";
  html += "<h3>LED Effects</h3>";
  html += "<div class='effects-grid'>";
  html += "<button onclick='setEffect(\"rainbow\")' class='btn effect-btn'>Rainbow (HSV)</button>";
  html += "<button onclick='setEffect(\"fill_rainbow\")' class='btn effect-btn'>Rainbow (Fill)</button>";
  html += "<button onclick='setEffect(\"static\")' class='btn effect-btn'>Static Color</button>";
  html += "<button onclick='setEffect(\"snake\")' class='btn effect-btn'>Snake</button>";
  html += "<button onclick='returnToMonitoring()' class='btn btn-monitoring'>Return to Monitoring</button>";
  html += "</div>";
  html += "<div id='colorPicker' style='display:none; margin-top:10px;'>";
  html += "<label>Static Color: </label>";
  html += "<input type='color' id='staticColor' value='" + staticColor + "' onchange='updateStaticColor()'>";
  html += "</div>";
  html += "<div id='snakeColorPicker' style='display:none; margin-top:10px;'>";
  html += "<label>Snake Color: </label>";
  html += "<input type='color' id='snakeColor' value='" + snakeColor + "' onchange='updateSnakeColor()'>";
  html += "</div>";
  html += "</div>";

  // Reset Section
  html += "<div class='section'>";
  html += "<h3>Factory Reset</h3>";
  html += "<button onclick='factoryReset()' class='btn btn-danger'>Reset to Factory Settings</button>";
  html += "</div>";

  html += "</div>";
  html += "<script>";
  html += "function setEffect(effect) {";
  html += "  document.getElementById('colorPicker').style.display = 'none';";
  html += "  document.getElementById('snakeColorPicker').style.display = 'none';";
  html += "  if(effect === 'static') {";
  html += "    document.getElementById('colorPicker').style.display = 'block';";
  html += "  } else if(effect === 'snake') {";
  html += "    document.getElementById('snakeColorPicker').style.display = 'block';";
  html += "  }";
  html += "  fetch('/effect', {";
  html += "    method: 'POST',";
  html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
  html += "    body: `effect=${effect}&color=${document.getElementById('staticColor').value}&snakeColor=${document.getElementById('snakeColor').value}`";
  html += "  });";
  html += "}";
  html += "function updateStaticColor() {";
  html += "  setEffect('static');";
  html += "}";
  html += "function updateSnakeColor() {";
  html += "  setEffect('snake');";
  html += "}";
  html += "function returnToMonitoring() {";
  html += "  fetch('/monitoring', {method: 'POST'}).then(() => {";
  html += "    alert('Returned to monitoring mode');";
  html += "  });";
  html += "}";
  html += "function factoryReset() {";
  html += "  if(confirm('Are you sure you want to reset to factory settings?')) {";
  html += "    fetch('/reset', {method: 'POST'}).then(() => {";
  html += "      alert('Device will restart in factory mode');";
  html += "      setTimeout(() => location.reload(), 3000);";
  html += "    });";
  html += "  }";
  html += "}";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleCSS()
{
  String css = "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0}";
  css += ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
  css += "h1{color:#333;text-align:center;margin-bottom:10px}";
  css += "h2{color:#666;text-align:center;margin-bottom:30px}";
  css += "h3{color:#444;border-bottom:2px solid #007bff;padding-bottom:5px}";
  css += ".section{margin-bottom:30px;padding:20px;background:#f8f9fa;border-radius:8px}";
  css += ".btn{padding:10px 20px;border:none;border-radius:5px;cursor:pointer;font-size:14px;margin:5px}";
  css += ".btn:hover{opacity:0.8}";
  css += ".btn{background:#007bff;color:white}";
  css += ".btn-primary{background:#28a745}";
  css += ".btn-danger{background:#dc3545}";
  css += ".btn-monitoring{background:#fd7e14}";
  css += ".effect-btn{background:#17a2b8;margin:5px}";
  css += ".effects-grid{display:flex;flex-wrap:wrap;gap:10px}";
  css += "input[type='text'],input[type='password']{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box}";
  css += "input[type='color']{width:60px;height:40px;border:none;border-radius:5px;cursor:pointer}";
  css += "label{font-weight:bold;margin-right:10px}";
  css += ".networks{margin-top:10px}";
  css += ".network-item{padding:10px;margin:5px 0;background:white;border-radius:5px;cursor:pointer;border:1px solid #ddd}";
  css += ".network-item:hover{background:#e9ecef}";
  css += ".status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px}";
  css += ".status-item{display:flex;justify-content:space-between;padding:10px;background:white;border-radius:5px}";
  css += ".label{font-weight:bold;color:#666}";
  css += ".value{color:#333}";
  css += ".connected{color:#28a745!important;font-weight:bold}";
  css += ".disconnected{color:#dc3545!important;font-weight:bold}";
  css += "@media(max-width:600px){.container{padding:10px}.effects-grid{flex-direction:column}}";

  server.send(200, "text/css", css);
}

void handleWiFiScan()
{
  String html = "";
  int n = WiFi.scanNetworks();

  if (n == 0)
  {
    html = "<p>No networks found</p>";
  }
  else
  {
    html = "<div class='networks'>";
    for (int i = 0; i < n; ++i)
    {
      html += "<div class='network-item' onclick='selectNetwork(\"" + WiFi.SSID(i) + "\")'>";
      html += "<strong>" + WiFi.SSID(i) + "</strong>";
      html += " (" + String(WiFi.RSSI(i)) + " dBm)";
      if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN)
      {
        html += " 🔒";
      }
      html += "</div>";
    }
    html += "</div>";
  }

  server.send(200, "text/html", html);
}

void handleWiFiConnect()
{
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  if (ssid.length() == 0)
  {
    server.send(400, "text/plain", "SSID is required");
    return;
  }

  Serial.println("Attempting to connect to: " + ssid);

  // Try to connect
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  // Wait for connection
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    // Save credentials
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);

    server.send(200, "text/plain", "Success! Connected to " + ssid + ". Device will restart in monitoring mode.");

    delay(2000);
    ESP.restart();
  }
  else
  {
    server.send(400, "text/plain", "Failed to connect to " + ssid + ". Please check credentials.");
    // Restart AP mode
    startFactoryMode();
  }
}

void handleEffectChange()
{
  String effect = server.arg("effect");
  String color = server.arg("color");
  String snakeColorArg = server.arg("snakeColor");

  currentEffect = effect;
  if (color.length() > 0)
  {
    staticColor = color;
  }
  if (snakeColorArg.length() > 0)
  {
    snakeColor = snakeColorArg;
  }

  Serial.println("Effect changed to: " + effect);
  server.send(200, "text/plain", "Effect changed to " + effect);
}

void handleFactoryResetWeb()
{
  preferences.clear();
  server.send(200, "text/plain", "Factory reset initiated. Device will restart.");
  delay(1000);
  ESP.restart();
}

void handleMonitoringMode()
{
  currentEffect = "monitoring";
  Serial.println("Returned to monitoring mode");
  server.send(200, "text/plain", "Returned to monitoring mode");
}

void handleStatus()
{
  String json = "{";
  json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"internet\":" + String(internetStatus ? "true" : "false") + ",";
  json += "\"effect\":\"" + currentEffect + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void checkFactoryReset()
{
  if (digitalRead(RESET_PIN) == LOW)
  {
    if (!factoryResetPressed)
    {
      factoryResetPressed = true;
      factoryResetPressTime = millis();
    }
    else if (millis() - factoryResetPressTime > 20)
    { // Hold for 3 seconds
      Serial.println("Factory reset button pressed!");
      preferences.clear();
      ESP.restart();
    }
  }
  else
  {
    factoryResetPressed = false;
  }
}

void checkInternetConnection()
{
  HTTPClient http;
  http.begin("http://clients3.google.com/generate_204");
  http.setTimeout(3000); // трохи менший таймаут
  int httpCode = http.GET();
  bool wasConnected = internetStatus;
  internetStatus = (httpCode == 204);
  http.end();

  Serial.printf("Internet check: %s (code=%d)\n", internetStatus ? "Connected" : "Disconnected", httpCode);

  if (deviceMode == "monitoring" && (currentEffect == "monitoring" || currentEffect == "breathe_green" || currentEffect == "blink_red"))
  {
    currentEffect = internetStatus ? "breathe_green" : "blink_red";
  }
}

void updateLEDEffects()
{
  unsigned long currentTime = millis();

  if (currentTime - lastEffectUpdate < 50)
    return; // Update at ~20 FPS
  lastEffectUpdate = currentTime;

  if (currentEffect == "rainbow")
  {
    effectRainbow();
  }
  else if (currentEffect == "fill_rainbow")
  {
    effectFillRainbow();
  }
  else if (currentEffect == "static")
  {
    effectStatic();
  }
  else if (currentEffect == "snake")
  {
    effectSnake();
  }
  else if (currentEffect == "waiting")
  {
    effectWaiting();
  }
  else if (currentEffect == "breathe_green" || currentEffect == "monitoring")
  {
    effectBreatheGreen();
  }
  else if (currentEffect == "blink_red")
  {
    effectBlinkRed();
  }

  FastLED.show();
}

void effectRainbow()
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CHSV((effectHue + i * 255 / NUM_LEDS) % 255, 255, 255);
  }
  effectHue += 2;
}

void effectFillRainbow()
{
  fill_rainbow(leds, NUM_LEDS, effectHue, 7);
  effectHue += 2;
}

void effectStatic()
{
  // Convert hex color to RGB
  long color = strtol(staticColor.substring(1).c_str(), NULL, 16);
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;

  fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
}

void effectSnake()
{
  fadeToBlackBy(leds, NUM_LEDS, 50);

  // Convert hex color to RGB for snake
  long color = strtol(snakeColor.substring(1).c_str(), NULL, 16);
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;

  leds[snakePosition] = CRGB(r, g, b);

  snakePosition += snakeDirection;
  if (snakePosition >= NUM_LEDS - 1 || snakePosition <= 0)
  {
    snakeDirection *= -1;
  }
}

void effectWaiting()
{
  // Soft rainbow wave
  for (int i = 0; i < NUM_LEDS; i++)
  {
    uint8_t brightness = beatsin8(20, 100, 255, 0, i * 10);
    leds[i] = CHSV((effectHue + i * 20) % 255, 200, brightness);
  }
  effectHue += 1;
}

void effectBreatheGreen()
{
  // інвертуємо лише при виході за межі
  if (breatheBrightness >= 255)
  {
    breatheDirection = -1;
  }
  else if (breatheBrightness <= 50)
  {
    breatheDirection = 1;
  }
  breatheBrightness += breatheDirection * 3;
  fill_solid(leds, NUM_LEDS, CHSV(96, 255, breatheBrightness));
}

void effectBlinkRed()
{
  static bool blinkState = false;
  static unsigned long lastBlink = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastBlink > 250)
  { // Blink every 250ms
    blinkState = !blinkState;
    lastBlink = currentTime;
  }

  if (blinkState)
  {
    fill_solid(leds, NUM_LEDS, CRGB::Red);
  }
  else
  {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
  }
}
