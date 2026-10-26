// ===== Mini-projet IoT : Irrigation intelligente =====
// Étape 1 : lecture des capteurs + test de la vanne

#include "DHTesp.h"       // bibliothèque du capteur DHT22
#include <ESP32Servo.h>   // bibliothèque du servomoteur

// --- 1) Les broches utilisées ---
const int PIN_DHT   = 15;  // DHT22 (température + humidité de l'air)
const int PIN_SOL   = 34;  // potentiomètre = humidité du sol
const int PIN_PH    = 35;  // potentiomètre = pH du sol
const int PIN_LUM   = 32;  // photorésistance = luminosité
const int PIN_VANNE = 18;  // servomoteur = vanne

// --- 2) Les objets qui représentent les composants ---
DHTesp dht;
Servo vanne;

// --- 3) setup() : s'exécute UNE SEULE FOIS au démarrage ---
void setup() {
  Serial.begin(115200);                 // ouvre le moniteur série
  dht.setup(PIN_DHT, DHTesp::DHT22);    // prépare le DHT22
  vanne.attach(PIN_VANNE, 500, 2400);   // prépare le servomoteur
  vanne.write(0);                       // vanne fermée au départ
  Serial.println("=== Systeme d'irrigation : demarrage ===");
}

// --- 4) loop() : se répète à l'infini ---
void loop() {
  // a) Lire le DHT22
  TempAndHumidity mesure = dht.getTempAndHumidity();
  float temperature = mesure.temperature;   // en °C
  float humiditeAir = mesure.humidity;      // en %

  // b) Lire les capteurs analogiques (valeur brute entre 0 et 4095)
  //    puis la convertir dans une unité compréhensible
  int   humiditeSol = map(analogRead(PIN_SOL), 0, 4095, 0, 100);  // en %
  float ph          = analogRead(PIN_PH) * 14.0 / 4095.0;         // de 0 à 14
  int   luminosite  = map(analogRead(PIN_LUM), 4095, 0, 0, 100);  // en %

  // c) Règle TEMPORAIRE (l'IA la remplacera plus tard) :
  //    si le sol est sec (< 30 %), on ouvre la vanne
  bool vanneOuverte = (humiditeSol < 30);
  vanne.write(vanneOuverte ? 90 : 0);

  // d) Afficher les valeurs dans le moniteur série
  Serial.println("--------------------------------");
  Serial.printf("Temperature air : %.1f C\n", temperature);
  Serial.printf("Humidite air    : %.1f %%\n", humiditeAir);
  Serial.printf("Humidite sol    : %d %%\n", humiditeSol);
  Serial.printf("pH sol          : %.1f\n", ph);
  Serial.printf("Luminosite      : %d %%\n", luminosite);
  Serial.printf("Vanne           : %s\n", vanneOuverte ? "OUVERTE" : "FERMEE");

  delay(2000);  // attendre 2 secondes
}