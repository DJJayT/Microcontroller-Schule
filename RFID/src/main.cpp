/*
 * Bibliothek RFID_MFRC522v2 von GithubCommunity
 *
 * Anschlussbelegung:
 * ==================
 * NodeMCU | MF-RC522
 *      D4 | SDA (SS)
 *      D5 | SCK
 *      D7 | MOSI
 *      D6 | MISO (SCL)
 *      D3 | RST
 *      3V | 3V
 *     GND | GND
 *
 *
 * NodeMCU | Buzzer
 *     D8 | S
 *     GND | GND
 *     3V | VCC
 */

// #include <SPI.h>      // für SPI-Schnittstelle
#include <MFRC522v2.h> // für den Kartenleser
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#define DEBUG false  // auf true setzen für Debug-Ausgaben

// Einstellungen für Kartenleser
MFRC522DriverPinSimple ss_pin(D4);
SPIClass &spiClass = SPI;
const SPISettings spiSettings = SPISettings(SPI_CLOCK_DIV4, MSBFIRST, SPI_MODE0);
MFRC522DriverSPI driver{ss_pin, spiClass, spiSettings};
MFRC522 mfrc522{driver};


const unsigned long sameCardInterval = 500;  // Intervall in dem die gleiche Karte nicht nochmal behandelt werden soll
char lastCard[4];  // Variable für die zuletzt gelesene Karten-ID
unsigned long lastCardMillis = 0;  // Variable zur Speicherung des Zeitstempels für das sameCardInterval

// forward declarations
void setup_wifi();
void callback(String topic, byte *message, unsigned int length);
void reconnect();

// WiFi-Zugangsdaten
const char *ssid = "BS-GAST";
const char *passphrase = "bs2020##";

// MQTT-Serverdaten
const char *mqtt_server = "mqtt.fachinformatiker.schule";
const int mqtt_port = 8883; // für eine TLS-verschlüsselte Kommunikation
const char *mqtt_user = "itt11d";
const char *mqtt_password = "ZumRegelwidrigenRegen";
const char *mqtt_client_name = "JayEricRFID"; // beliebig wählbar, muss aber pro Client einmalig sein (wegen Mosquitto); bitte ändern

// Topics für Subscription und eigenes Publishing
const char *topic_sub = "itt11d/JayEricSchranke/CardValidation";
const char *topic_pub = "itt11d/JayEricSchranke/karte_id";

// Konstanten
const int pub_interval = 10000; // Intervall für eigene pub-Nachrichten (in ms)

WiFiClientSecure wifi;
PubSubClient client(wifi);

long now;      // aktuelle Laufzeit
long last = 0; // Zeitpunkt des letzten MQTT-Publish

int pub_counter = 0; // Zählwert als Inhalt für eigene Publish-Nachrichten

/**
 * setup
 */
void setup() {
    // Kartenleser initialisieren
    mfrc522.PCD_Init();
    delay(100);
    pinMode(D8, OUTPUT);  // Buzzer-Pin als Ausgang definieren

  // Serielle Konsole aktivieren und Versionsinfo ausgeben
  Serial.begin(9600);
  Serial.println();
  Serial.println("Version 1.1");

  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);

  // Serial
  delay(100);

  // WiFi
  setup_wifi();

  // MQTT
  wifi.setInsecure(); // MQTT-Kommunikation ohne TLS-Zertifikatsvalidierung ermöglichen
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("Setup abgeschlossen.");
}


/**
 * loop
 */
void loop()
{
  if (!client.connected())
  {
    // MQTT-Verbindung wieder herstellen
    reconnect();
  }

  // regelmäßigen Nachrichtenempfang gewährleisten, false bei Verbindungsabbruch
  if (!client.loop())
  {
    client.connect(mqtt_client_name, mqtt_user, mqtt_password);
  }

  // non-blocking Timer für eigene Publish-Nachrichten;
  now = millis();

  // falls keine Karte erkannt wird oder gelesen werden kann, dann von vorne
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
  {
    return;
  }
  else // weiter falls Karte gelesen
  {
    char thisCard[4];           // Variable für die Karten-UID
    bool sameAsLastCard = true; // Variable um zu speichern, ob das eine neue Karte ist


        // Karten-UID in lokale Variable übertragen und erkennen, ob es eine neue ist
        for (int i = 0; i < 4; i++) {
            thisCard[i] = mfrc522.uid.uidByte[i];  // je ein einzelnes Byte kopieren
            if (thisCard[i] != lastCard[i])        // und auf Unterschied zur letzten Karte prüfen
            {
                sameAsLastCard = false;
            }
        }


        // ausführen bei einer neuen Karte oder nach Ablauf des Wiederholungsintervalls
        if (!sameAsLastCard || ((millis() - lastCardMillis) > sameCardInterval)) {
            bool matchesSomeValidCard = false;  // Variable um zu speichern, ob diese Karte einer gültigen entspricht

            // UID der neu gelesenen Karte merken, um sie nicht mehrmals hintereinander zu prüfen
            // und Ausgabe der ID der neu erkannten Karte
            for (int i = 0; i < 4; i++) {
                lastCard[i] = thisCard[i];
                Serial.print(thisCard[i], HEX);
                Serial.print(" ");
            }
            lastCardMillis = millis();  // Zeitstempel für das Widerholungsintervall merken
            Serial.println();

            client.publish(topic_pub, thisCard);
        }  // Ende neue Karte
    }  // Ende Karte gelesen


    if (DEBUG) {
        MFRC522Debug::PICC_DumpToSerial(mfrc522, Serial, &(mfrc522.uid));
    }

    delay(100);
}

// Setup Wifi

void setup_wifi()
{
  delay(10);
  Serial.print("WiFi");
  WiFi.begin(ssid, passphrase);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" connected");
}

/**
 * callback
 */
void callback(String topic, byte *message, unsigned int length)
{
  // Callback-Funktion für MQTT-Subscriptions
  String messageTemp;

  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  // Message byteweise verarbeiten und auf Konsole ausgeben
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Behandlung bestimmter Topics
  if (topic == topic_sub)
  {
      if (messageTemp == "true") {
          Serial.println("Card is valid");
          for (int i = 0; i < 400; i++) {
              digitalWrite(D8, HIGH);
              delayMicroseconds(300);
              digitalWrite(D8, LOW);
              delayMicroseconds(300);
          }
      } else {
          Serial.println("Card is NOT valid");
          for (int i = 0; i < 200; i++) {
              digitalWrite(D8, HIGH);
              delayMicroseconds(300);
              digitalWrite(D8, LOW);
              delayMicroseconds(300);
          }
          for (int i = 0; i < 300; i++) {
              digitalWrite(D8, HIGH);
              delayMicroseconds(500);
              digitalWrite(D8, LOW);
              delayMicroseconds(500);
          }
      }
  }
  /* else if (topic == anderes_topic) { ... } */
}

/**
 * reconnect
 */
void reconnect()
{
  // Wiederverbinden mit dem MQTT-Broker
  while (!client.connected())
  {
    Serial.println("MQTT reconnect");
    if (client.connect(mqtt_client_name, mqtt_user, mqtt_password))
    {
      Serial.println("connected");
      client.subscribe(topic_sub);
    }
    else
    {
      Serial.print("failed: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}
