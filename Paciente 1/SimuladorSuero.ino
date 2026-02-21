/**
 * SIMULADOR DE MONITOREO DE SUERO
 * 
 * NOTA IMPORTANTE: ThingSpeak gratuito solo admite una actualización cada 15s.
 * Este código actualiza el nivel interno cada 1s, pero solo envía a la nube cada 20s.
 */

#include <WiFi.h>
#include <ThingSpeak.h> // Librería oficial de MathWorks

// ---------------- CONFIGURACIÓN DE USUARIO (CAMBIAR POR PACIENTE) ----------------
// CADA PESTAÑA DE WOKWI DEBE TENER UN CHANNEL_ID Y API_KEY DIFERENTE

unsigned long MY_CHANNEL_NUMBER = 3268470;  // <--- ID DE CANAL (CH1-3268470, CH2-3268473,CH3-3268474,CH4-3268476)
const char * MY_WRITE_API_KEY = "GYNAD2410DZVNEOV"; // <--- API KEY (CH1-GYNAD2410DZVNEOV, CH2-HEEKBOROMQ3GPTWV,CH3-WC8I8JHQBQTKLTHX,CH4-WY8KEL4DN4ZORMTK)

// ---------------- CONFIGURACIÓN DE RED (NO CAMBIAR EN WOKWI) ----------------
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ---------------- PARÁMETROS DE SIMULACIÓN ----------------
const int MIN_WAIT_MIN = 2; // Minutos de espera inicial
const int MAX_WAIT_MIN = 5; // Maximos de espera inicial
const int CLOUD_UPDATE_INTERVAL = 20000; // 20 segundos (Mínimo ThingSpeak es 15s)

// ---------------- VARIABLES DEL SISTEMA ----------------
WiFiClient client;
float serumLevel = 100.0;
unsigned long waitDurationMs = 0;
unsigned long stateStartTime = 0;
unsigned long lastCloudUpdate = 0;
unsigned long lastSimulationStep = 0;
bool isFinished = false;
float drain_rate = 1.0;

enum State { CONNECTING, STARTING, DRAINING, FINISHED };
State currentState = CONNECTING;

void setup() {
  Serial.begin(115200);
  
  // 1. Conexión WiFi
  Serial.print("Conectando a WiFi Wokwi-GUEST");
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6); // Canal 6 optimiza conexión en Wokwi
  while (WiFi.status()!= WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado. IP: " + WiFi.localIP().toString());

  // 2. Inicializar ThingSpeak
  ThingSpeak.begin(client);

  // 3. Calcular tiempo de espera aleatorio
  // Usamos ruido analógico + millis para semilla aleatoria real
  randomSeed(analogRead(34) + millis());
  int waitMinutes = random(MIN_WAIT_MIN, MAX_WAIT_MIN + 1);
  waitDurationMs = waitMinutes * 60 * 1000;
  drain_rate = (100.0/(waitDurationMs/1000.0)); // Porcentaje de suero que disminuye por SEGUNDO

  Serial.println("-------------------------------------------");
  Serial.print("PACIENTE LISTO. TIEMPO DE ESPERA ALEATORIO: ");
  Serial.print(waitMinutes);
  Serial.println(" MINUTOS.");
  Serial.print("TIEMPO DE ESPERA ALEATORIO: ");
  Serial.print(waitDurationMs);
  Serial.println(" ms.");
  Serial.println("-------------------------------------------");
  Serial.print("DRAIN RATE: ");
  Serial.println(drain_rate);

  currentState = STARTING;
  stateStartTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  switch (currentState) {
    
    // ESTADO 1: INICIO
    case STARTING:
      // Comienza la simulación de infusión de suero
      Serial.println("-------------------------------------------");
      Serial.println("¡INICIO DE INFUSIÓN! El suero comienza a bajar.");
      Serial.println("-------------------------------------------");
      Serial.print("Nivel Suero: ");
      Serial.print(serumLevel);
      Serial.println("%");
      Serial.println("-------------------------------------------");

      //Envío de nivel inicial
      sendToThingSpeak(serumLevel);

      currentState = DRAINING;

      break;

    // ESTADO 2: DRENAJE ACTIVO
    case DRAINING:
      // A. Simulación Interna (Cada 1 segundo)
      if (currentMillis - lastSimulationStep >= 1000) {
        lastSimulationStep = currentMillis;
        serumLevel -= drain_rate;
        if (serumLevel < 0) serumLevel = 0;
        
        Serial.print("Nivel Suero: ");
        Serial.print(serumLevel);
        Serial.println("%");

        if (serumLevel <= 0) {
          currentState = FINISHED;
        }
      }

      // B. Envío a la Nube (Cada 20 segundos)
      if (currentMillis - lastCloudUpdate >= CLOUD_UPDATE_INTERVAL) {
        sendToThingSpeak(serumLevel);
        lastCloudUpdate = currentMillis;
      }
      break;

    // ESTADO 3: FINALIZADO
    case FINISHED:
      if (!isFinished) {
        if (currentMillis - lastCloudUpdate >= CLOUD_UPDATE_INTERVAL) {
          Serial.println("SUERO FINALIZADO. Enviando última alerta...");
          sendToThingSpeak(0.0); // Asegurar que llegue el 0 exacto
          isFinished = true;
        }
      }
      // Ya no enviamos más para no saturar ni gastar mensajes
      break;
  }
}

void sendToThingSpeak(float value) {
  Serial.print(">>> ENVIANDO A THINGSPEAK (Field 1): ");
  Serial.println(value);
  
  // Escribir en Campo 1
  int x = ThingSpeak.writeField(MY_CHANNEL_NUMBER, 1, value, MY_WRITE_API_KEY);
  
  if(x == 200){
    Serial.println(">>> ÉXITO: Dato recibido en nube.");
  } else {
    Serial.println(">>> ERROR: Código HTTP " + String(x));
    if (x == -401) Serial.println(" (Revise API KEY)");
    if (x == 0) Serial.println(" (Error de conexión)");
  }
}

