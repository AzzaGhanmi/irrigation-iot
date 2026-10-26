// ===== Mini-projet IoT : Irrigation intelligente =====
// Étape 2 : envoi des mesures vers Firebase + lecture de la commande de la vanne

#include <WiFi.h>              // pour se connecter au Wi-Fi
#include <WiFiClientSecure.h>  // pour les connexions sécurisées (https)
#include <HTTPClient.h>        // pour envoyer / recevoir des données sur Internet
#include "DHTesp.h"
#include <ESP32Servo.h>

// --- 1) Paramètres Wi-Fi (le Wi-Fi virtuel de Wokwi) ---
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// --- 2) Adresse de TA base Firebase ---
const String FIREBASE_URL = "https://irrigation-iot-3b3ed-default-rtdb.europe-west1.firebasedatabase.app";

// --- 3) Les broches ---
const int PIN_DHT   = 15;
const int PIN_SOL   = 34;
const int PIN_PH    = 35;
const int PIN_LUM   = 32;
const int PIN_VANNE = 18;

DHTesp dht;
Servo vanne;
WiFiClientSecure client;

// ---------- Fonction : se connecter au Wi-Fi ----------
void connecterWifi() {
  Serial.print("Connexion au WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(" connecte !");
}

// ---------- Fonction : ÉCRIRE une donnée dans Firebase ----------
// chemin = l'endroit dans l'arbre (ex : "/capteurs")
// json   = la donnée à écrire
bool ecrireFirebase(String chemin, String json) {
  HTTPClient http;
  http.begin(client, FIREBASE_URL + chemin + ".json");
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(json);   // PUT = remplacer la valeur à cet endroit
  http.end();
  return (code == 200);        // 200 = "tout s'est bien passé"
}

// ---------- Fonction : LIRE une donnée dans Firebase ----------
String lireFirebase(String chemin) {
  HTTPClient http;
  http.begin(client, FIREBASE_URL + chemin + ".json");
  int code = http.GET();       // GET = lire la valeur à cet endroit
  String reponse = "";
  if (code == 200) reponse = http.getString();
  http.end();
  reponse.replace("\"", "");   // enlève les guillemets : "FERMEE" -> FERMEE
  reponse.trim();
  return reponse;
}

// ---------- setup : une seule fois au démarrage ----------
void setup() {
  Serial.begin(115200);
  dht.setup(PIN_DHT, DHTesp::DHT22);
  vanne.attach(PIN_VANNE, 500, 2400);
  vanne.write(0);
  client.setInsecure();        // simplifie la connexion https (suffisant pour le projet)
  Serial.println("=== Systeme d'irrigation : demarrage ===");
  connecterWifi();
}

// ---------- loop : se répète toutes les 5 secondes ----------
void loop() {
  // a) Si le Wi-Fi a été perdu, on se reconnecte
  if (WiFi.status() != WL_CONNECTED) connecterWifi();

  // b) Lire les capteurs (comme à l'étape 1)
  TempAndHumidity mesure = dht.getTempAndHumidity();
  float temperature = mesure.temperature;
  float humiditeAir = mesure.humidity;
  int   humiditeSol = map(analogRead(PIN_SOL), 0, 4095, 0, 100);
  float ph          = analogRead(PIN_PH) * 14.0 / 4095.0;
  int   luminosite  = map(analogRead(PIN_LUM), 4095, 0, 0, 100);

  // c) Construire le message JSON avec toutes les mesures
  //    {".sv":"timestamp"} demande à Firebase d'ajouter l'heure exacte
  char json[256];
  snprintf(json, sizeof(json),
    "{\"temperature\":%.1f,\"humidite_air\":%.1f,\"humidite_sol\":%d,"
    "\"ph\":%.1f,\"luminosite\":%d,\"timestamp\":{\".sv\":\"timestamp\"}}",
    temperature, humiditeAir, humiditeSol, ph, luminosite);

  // d) Envoyer les mesures dans Firebase
  bool envoiOK = ecrireFirebase("/capteurs", json);

  // e) Lire la commande de la vanne dans Firebase et obéir
  String commande = lireFirebase("/vanne/commande");
  bool vanneOuverte = (commande == "OUVERTE");
  vanne.write(vanneOuverte ? 90 : 0);

  // f) Écrire l'état réel de la vanne dans Firebase
  ecrireFirebase("/vanne/etat", vanneOuverte ? "\"OUVERTE\"" : "\"FERMEE\"");

  // g) Afficher dans le moniteur série
  Serial.println("--------------------------------");
  Serial.printf("Temp: %.1f C | Hum air: %.1f %% | Sol: %d %% | pH: %.1f | Lum: %d %%\n",
                temperature, humiditeAir, humiditeSol, ph, luminosite);
  Serial.printf("Envoi Firebase : %s\n", envoiOK ? "OK" : "ERREUR");
  Serial.printf("Commande lue   : %s  ->  Vanne %s\n",
                commande.c_str(), vanneOuverte ? "OUVERTE" : "FERMEE");

  delay(5000);
}