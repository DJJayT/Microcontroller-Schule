/*
 * Dieses Beispiel implementiert einen MQTT-Client mit Hilfe der PubSubClient-Library. Ergänzend zu den online verfügbaren Beispielen
 * werden hier Zugangsdaten für den MQTT-Broker verwendet. Die Beispieldaten müssen durch die tatsächlichen Zugangsdaten ersetzt werden.
 *
 * Das Beispiel beinhaltet sowohl MQTT-Subscribe als auch MQTT-Publish.
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// forward declarations
void setup_wifi();
void callback(String topic, byte* message, unsigned int length);
void reconnect();

// WiFi-Zugangsdaten
const char* ssid = "BS-GAST";
const char* passphrase = "bs2020##";

// MQTT-Serverdaten
const char* mqtt_server = "mqtt.fachinformatiker.schule";
const int mqtt_port = 8883; // für eine TLS-verschlüsselte Kommunikation
const char* mqtt_user = "itt11d";
const char* mqtt_password = "ZumRegelwidrigenRegen";
const char* mqtt_client_name = "JayEricSchranke";  // beliebig wählbar, muss aber pro Client einmalig sein (wegen Mosquitto); bitte ändern

// Topics für Subscription und eigenes Publishing
const char* topic_sub = "itt11d/JayEricSchranke/Schranke";
const char* topic_pub = "itt11d/JayEricSchranke/Schrankenstatus";

// Konstanten
const int pub_interval = 1000;  // Intervall für eigene pub-Nachrichten (in ms)


WiFiClientSecure wifi;
PubSubClient client(wifi);

long now;  // aktuelle Laufzeit
long last = 0;  // Zeitpunkt des letzten MQTT-Publish

int pub_counter = 0;  // Zählwert als Inhalt für eigene Publish-Nachrichten


/* ---------- Hauptfunktionen ---------- */

/**
 * setup
 */
void setup() {
    // Serial
    Serial.begin(9600);
    Serial.println();
    delay(100);
    Serial.println("Setup...");

    // WiFi
    setup_wifi();

    // MQTT
    wifi.setInsecure(); // MQTT-Kommunikation ohne TLS-Zertifikatsvalidierung ermöglichen
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    Serial.println("Setup abgeschlossen");
}


/**
 * loop
 */
void loop() {
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
    if (millis() - last > pub_interval)
    {
        last = now;
        //pub_counter++;
        //client.publish(topic_pub, String(pub_counter).c_str());
        //Serial.print("Publish: ");
        //Serial.print(topic_pub);
        //Serial.print(": ");
        //Serial.println(pub_counter);
    }
}


/* ---------- andere Funktionen ---------- */

/**
 * setup_wifi
 */
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
void callback(String topic, byte* message, unsigned int length)
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
        if (messageTemp == "on")
        {
            Serial.print(topic_sub);
            Serial.println(" steht auf EIN");
        }
        else if (messageTemp == "off")
        {
            Serial.print(topic_sub);
            Serial.println(" steht auf AUS");
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