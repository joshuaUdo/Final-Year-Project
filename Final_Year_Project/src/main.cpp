#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <SPIFFS.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiClientSecure.h>

// WiFi credentials
// const char* ssid = "iPhone 14 Plus";
// const char* password = "jbgadget2023";
const char* ssid = "DESKTOP-U8V8KPC";
const char* password = ")08U5q13";

//Database credentials
const char* supabaseUrl = "https://gqegluzncbgikuwngehf.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImdxZWdsdXpuY2JnaWt1d25nZWhmIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzE5MjE3MTksImV4cCI6MjA4NzQ5NzcxOX0.MUVnUxSmkCzkKZz0kFuQ8HmUhE3k_pNBmwi8YDaJwLA";

//RFID Pins
#define SS_PIN 5
#define RST_PIN 4

//OLED Pins
#define OLED_SDA 21
#define OLED_SCL 22

//OLED Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

//Offline backup file path
#define OFFLINE_FILE "/offline_logs.csv"

//Time settings using NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset = 0;

//component initialization
MFRC522 mfrc522(SS_PIN, RST_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

HTTPClient http;

//To display messages on the OLED screen
void showMessage(String line1, String line2, int duration) {
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(line1);

  display.setTextSize(1);
  display.setCursor(0, 36);
  display.println(line2);

  display.display();

  if (duration > 1) delay(duration);
}

String getCurrentMealWindow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time");
    return "NONE";
  }
  int hour = timeinfo.tm_hour;
  if (hour >= 12 && hour <= 14) return "LUNCH";
  if (hour >= 16 && hour <= 17) return "DINNER";
  return "NONE";
}

String getCurrentTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00+01:00";
  }
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+01:00", &timeinfo);
  return String(buf);
}

void verifyMealAccess(String uidString) {
  // WiFiClientSecure client;
  // client.setInsecure();

  String url = String(supabaseUrl) + "/rest/v1/rpc/verify_meal_access";
  unsigned long t0 = millis();

  http.setReuse(true); // Enable connection reuse for better performance
  http.setTimeout(5000); // Set a timeout for the request
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", "Bearer " + String(supabaseKey));

  String payload = "{\"target_uid\": \"" + uidString + "\"}";
  int responseCode = http.POST(payload); //posts to supabase

  if(responseCode > 0){
    String response = http.getString();
    response.replace("\"", ""); // Remove quotes from response
    response.trim();
    Serial.println("Response: " + response);

    if (response == "ACCESS_GRANTED") {
      showMessage("ACCESS", "Enjoy your meal!", 3000);
    } else if (response == "ALREADY_EATEN") {
      showMessage("DENIED", "Already collected!", 3000);
    } else if (response == "NOT_MEAL_TIME") {
      showMessage("DENIED", "Outside meal hrs", 3000);
    } else if (response == "INVALID_CARD") {
      showMessage("DENIED", "Card not found", 3000);
    } else {
      showMessage("ERROR", "Try again", 3000);
    }
  }else {
    Serial.println("HTTP Error: " + String(responseCode));
    showMessage("HTTP ERROR", String(responseCode), 3000);
  }
  showMessage("System Ready", "Scan Card", 1);
}

void handleOfflineScan(String uidString){
  String mealType = getCurrentMealWindow();

  if (mealType == "NONE") {
    showMessage("DENIED", "Outside meal hrs", 3000);
    showMessage("System Ready", "Scan Card", 1);
    return;
  }

  String timestamp = getCurrentTimestamp();
  //save file to spiffs
  File file = SPIFFS.open(OFFLINE_FILE, FILE_APPEND);
  if (file){
    file.println(uidString + "," + mealType + "," + timestamp);
    file.close();
    Serial.println("Logged offline scan: " + uidString + ", " + mealType + ", " + timestamp);
    showMessage("OFFLINE", "Scan logged", 3000);
  }
  showMessage("No Wifi", "Offline Mode", 1);
}

void syncOfflineLogs() {
  if (!SPIFFS.exists(OFFLINE_FILE)) {
    Serial.println("No offline logs to sync");
    return;
  }

  File file = SPIFFS.open(OFFLINE_FILE, FILE_READ);
  if (!file) {
    Serial.println("Failed to open offline log for reading");
    return;
  }

  Serial.println("Syncing offline logs...");
  showMessage("Syncing", "Offline logs...", 1);

  int synced = 0;
  int failed = 0;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // Parse: uid,meal_type,timestamp
    int comma1 = line.indexOf(',');
    int comma2 = line.lastIndexOf(',');
    if (comma1 < 0 || comma2 <= comma1) continue;

    String uid       = line.substring(0, comma1);
    String mealType  = line.substring(comma1 + 1, comma2);
    String timestamp = line.substring(comma2 + 1);

    // POST to Supabase offline_logs table
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(supabaseUrl) + "/rest/v1/offline_logs";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", "Bearer " + String(supabaseKey));
    http.addHeader("Prefer", "return=minimal");

    String payload = "{\"rfid_uid\":\"" + uid +
                     "\",\"meal_type\":\"" + mealType +
                     "\",\"captured_at\":\"" + timestamp + "\"}";

    int responseCode = http.POST(payload);
    if (responseCode == 201) {
      synced++;
      Serial.println("Synced: " + uid);
    } else {
      failed++;
      Serial.println("Failed to sync: " + uid + " Code: " + responseCode);
    }
    http.end();
  }

  file.close();

  // If all synced successfully, delete the local file
  if (failed == 0 && synced > 0) {
    SPIFFS.remove(OFFLINE_FILE);
    Serial.println("All offline logs synced and cleared");
    showMessage("Sync Done", String(synced) + " records sent", 2000);
  } else if (synced > 0) {
    showMessage("Partial Sync", String(synced) + " ok, " + String(failed) + " fail", 2000);
  }

  showMessage("System Ready", "Scan Card...", 1);
}

void setup(){
  Serial.begin(115200);
  setCpuFrequencyMhz(240);

  //OLED init
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (true);
  }
  showMessage("Smart Cafeteria", "Starting...", 5);

  //SPIFFS Init
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS mounted");
  }

  //WiFi Init
  showMessage("Connecting", "WiFi...", 1);
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(100);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
    // Sync time via NTP
    configTime(gmtOffset_sec, daylightOffset, ntpServer);
    showMessage("WiFi Connected", WiFi.localIP().toString(), 2000);
    // Try to sync any offline logs
    syncOfflineLogs();
  } else {
    Serial.println("\nWiFi FAILED - Offline mode");
    showMessage("No WiFi", "Offline Mode", 2000);
  }

  // --- RFID Init ---
  SPI.begin(18, 19, 23, SS_PIN);
  mfrc522.PCD_Init();
  Serial.println("RFID Ready");

  showMessage("System Ready", "Scan Card...", 1);
}

void loop(){
  //   if (!mfrc522.PICC_IsNewCardPresent()) {
  //   return;
  // }
  // if (!mfrc522.PICC_ReadCardSerial()) {
  //   return;
  // }
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    static unsigned long lastSyncAttempt = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - lastSyncAttempt > 30000) { // Try syncing every 30 seconds
      syncOfflineLogs();
      lastSyncAttempt = millis();
    }
    return;
  }

  Serial.print("Card UID: ");

  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }

  uidString.toUpperCase();
  Serial.println("card scanned " + uidString);
  showMessage("Checking...", uidString, 1);
  
  if (WiFi.status() == WL_CONNECTED) {
    verifyMealAccess(uidString);
  } else {
    handleOfflineScan(uidString);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(3000);
  showMessage("System Ready", "Scan Card...", 1);
}