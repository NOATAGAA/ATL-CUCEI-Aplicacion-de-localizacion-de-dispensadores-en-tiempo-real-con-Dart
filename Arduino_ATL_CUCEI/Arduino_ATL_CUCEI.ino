#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "HX711.h"
#include <time.h> // Para obtener la fecha y hora

// Pin definition
const int LOADCELL_DOUT_PIN = 19;
const int LOADCELL_SCK_PIN = 23;

// HX711 instance creation
HX711 scale;

// WiFi credentials

#define WIFI_SSID "iCUCEI"
#define WIFI_PASSWORD ""

// Firebase configuration
#define API_KEY "******************" // API Key
#define DATABASE_URL "******************" // Database URL 
#define USER_EMAIL "*********" // Email address
#define USER_PASSWORD "**********" // Password

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Variables for daily consumption
float previousReading = 0.0;
float dailyConsumption = 0.0;
int lastDay = -1;

// Variable to store the last valid reading
float lastValidWeight = 0.0;

// Configuración NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600; // Zona horaria: UTC -6 (por ejemplo, México Central)
const int   daylightOffset_sec = 0;

void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to Wi-Fi");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  // Inicializamos la obtención de la hora vía NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("NTP client started");
}

void reconnectFirebase() {
  Firebase.reconnectWiFi(true);
  Serial.println("Attempting to reconnect to Firebase...");
}

// Función para obtener la hora y fecha actual
void getCurrentTime(int &year, int &month, int &day) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  year = timeinfo.tm_year + 1900;  // tm_year devuelve el año desde 1900
  month = timeinfo.tm_mon + 1;     // tm_mon va de 0 a 11, por eso se suma 1
  day = timeinfo.tm_mday;          // tm_mday es el día del mes
}

void setup() {
  Serial.begin(115200);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  // Set the scale factor
  float scale_factor = 23845.45;
  scale.set_scale(scale_factor);

  // Tare the scale only if it's the first time running
  if (lastValidWeight == 0.0) {
    scale.tare();
  }

  // Connect to Wi-Fi
  connectToWiFi();

  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Configure Firebase authentication
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Adjust SSL buffer sizes to optimize performance
  firebaseData.setBSSLBufferSize(512, 128); // Reduced buffer size
  firebaseData.setResponseSize(1024); // Reduced response size

  // Initialize Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
}

void loop() {
  // Check and reconnect Wi-Fi if necessary
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected, attempting to reconnect...");
    connectToWiFi();
  }

  // Check and reconnect Firebase if it's not ready
  if (!Firebase.ready()) {
    Serial.println("Firebase is not ready, attempting to reconnect...");
    reconnectFirebase();
    delay(5000); // Reduced delay to 5 seconds
    return;
  }

  if (scale.is_ready()) {
    float currentReading = scale.get_units(10);

    if (currentReading > 0) {
      lastValidWeight = currentReading;
    }

    Serial.print("Content: ");
    Serial.print(lastValidWeight, 1);
    Serial.println(" L");

    int year, month, day;
    getCurrentTime(year, month, day);

    if (day != lastDay) {
      if (lastDay != -1) {
        // Simplify the path for testing
        String path = "/ConsumosDiarios/D_moduloX/" + String(year) + "-" + String(month) + "-" + String(lastDay);
        if (Firebase.RTDB.setFloat(&firebaseData, path.c_str(), dailyConsumption)) {
          Serial.println("Daily consumption saved to Firebase");
        } else {
          Serial.print("Error saving daily consumption: ");
          Serial.println(firebaseData.errorReason());
          delay(5000); // Reduced delay
        }
      }

      dailyConsumption = 0.0;
      lastDay = day;
    }

    if (lastValidWeight <= previousReading) {
      dailyConsumption += (previousReading - lastValidWeight);
    }

    previousReading = lastValidWeight;

    // Simplify the path for testing
    String path = "/Dispensador/D_moduloX";
    if (Firebase.RTDB.setFloat(&firebaseData, path.c_str(), lastValidWeight)) {
      Serial.println("Data sent to Firebase");
    } else {
      Serial.print("Error sending data: ");
      Serial.println(firebaseData.errorReason());
      delay(5000); // Reduced delay
    }
  } else {
    Serial.println("Waiting for the sensor");
  }

  delay(10000); // Wait one minute before the next reading
}
