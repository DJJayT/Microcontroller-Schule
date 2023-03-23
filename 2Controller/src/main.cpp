/*
 *
 * Anschlussbelegung:
 * ==================
 *           NodeMCU | Schranke
 *                D8 | PWM
 *  5V anderes Board | 5V
 *               GND | GND
 *               GND | GND anderes Board
 *
 * NodeMCU | Display
 *    3.3V | VCC
 *     GND | GND
 *      D1 | SCL
 *      D2 | SDA
 *
 * Command to install display header: platformio lib install 2978
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <Wire.h>
#include <SSD1306.h>
#include <OLEDDisplay.h>

// forward declarations
void setup_wifi();

void callback(String topic, byte *message, unsigned int length);

void reconnect();

void schranke_auf();

void schranke_zu();

void schranke_zu_text();

// WiFi-Zugangsdaten
const char *ssid = "BS-GAST";
const char *passphrase = "bs2020##";

// MQTT-Serverdaten
const char *mqtt_server = "mqtt.fachinformatiker.schule";
const int mqtt_port = 8883; // für eine TLS-verschlüsselte Kommunikation
const char *mqtt_user = "itt11d";
const char *mqtt_password = "ZumRegelwidrigenRegen";
const char *mqtt_client_name = "JayEricSchranke";  // beliebig wählbar, muss aber pro Client einmalig sein (wegen Mosquitto); bitte ändern

// Topics für Subscription und eigenes Publishing
const char *topic_sub = "itt11d/JayEricSchranke/CardValidation";
const char *topic_pub = "itt11d/JayEricSchranke/Schrankenstatus";

const char *schranken_status = "closed";

// Konstanten
const int pub_interval = 1000;  // Intervall für eigene pub-Nachrichten (in ms)


WiFiClientSecure wifi;
PubSubClient client(wifi);

long now;  // aktuelle Laufzeit
long last_mqtt_publish = 0;  // Zeitpunkt des letzten MQTT-Publish

int pub_counter = 0;  // Zählwert als Inhalt für eigene Publish-Nachrichten

Servo servo;
int oldpos = 0;
int newpos = 0;
long since_time_opened = 0;
long opening_time = 5000;
bool is_open = false;

long since_access_revoke = 0;
long access_revoke_time = 5000;
bool access_revoked = false;

#define SERVO D8  // Servo an Pin D8
#define SDA_PIN D2  // SDA an D2
#define SCL_PIN D1  // SCL an D1

const int DISPLAY_ADDR = 0x3c;  // I2C-Addresse des Displays

SSD1306Wire *display;
//Display-Code muss noch komplett geaddet werden!


//SSD1306Wire *display;

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

    // Display
    display = new SSD1306Wire(DISPLAY_ADDR, SDA_PIN, SCL_PIN);
    display->init();
    display->displayOn();
    display->flipScreenVertically();
    display->clear();

    //Servo
    servo.attach(SERVO, 500, 2400);
    Serial.begin(115200);
    servo.write(newpos);

    //Starttext
    display->setFont(ArialMT_Plain_16);
    display->drawString(0, 0, "Schranke ist zu!");

    // Anzeige aktualisieren
    display->display();

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
    if (!client.connected()) {
        // MQTT-Verbindung wieder herstellen
        Serial.println(millis());
        Serial.println("Not connected");
        reconnect();
    }

    // regelmäßigen Nachrichtenempfang gewährleisten, false bei Verbindungsabbruch
    if (!client.loop()) {
        Serial.println("MQTT-Verbindung abgebrochen");
        client.connect(mqtt_client_name, mqtt_user, mqtt_password);
    }

    // non-blocking Timer für eigene Publish-Nachrichten;
    now = millis();
    if (millis() - last_mqtt_publish > pub_interval) {
        last_mqtt_publish = now;
        client.publish(topic_pub, schranken_status);
    }

    if (is_open) {
        if (millis() - since_time_opened > opening_time) {
            schranke_zu();
            schranke_zu_text();
        } else {
            display->clear();
            display->setFont(ArialMT_Plain_16);
            display->drawString(0, 0, "Schranke ist offen!");
            display->drawString(0, 30, "Noch " +
                                       String(((opening_time - (millis() - since_time_opened)) / 1000) + 1) +
                                       " Sek.");
            display->display();
        }
    } else if(access_revoked) {
        if(millis() - since_access_revoke > access_revoke_time) {
            schranke_zu_text();
            access_revoked = false;
        }
    }

    if (newpos != oldpos) {
        if (newpos >= 0 && newpos <= 180) {
            servo.write(newpos);
            oldpos = newpos;
        }
    }

    delay(50);
}

/**
 * setup_wifi
 */
void setup_wifi() {
    delay(10);
    Serial.print("WiFi");
    WiFi.begin(ssid, passphrase);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println(" connected");
}


/**
 * callback
 */
void callback(String topic, byte *message, unsigned int length) {
    // Callback-Funktion für MQTT-Subscriptions
    String messageTemp;

    Serial.print("Message arrived on topic: ");
    Serial.println(topic);

    // Message byteweise verarbeiten und auf Konsole ausgeben
    for (int i = 0; i < length; i++) {
        Serial.print((char) message[i]);
        messageTemp += (char) message[i];
    }
    Serial.println();

    // Behandlung bestimmter Topics
    if (topic == topic_sub) {
        Serial.println(messageTemp);
        if (messageTemp == "true") {
            schranke_auf();
        } else {
            schranke_zu();

            display->clear();
            display->setFont(ArialMT_Plain_16);
            display->drawString(0, 0, "Schranke ist zu!");
            display->drawString(0, 30, "Zutritt verweigert!");
            display->display();
            since_access_revoke = millis();
            access_revoked = true;
        }
    }
}


/**
 * reconnect
 */
void reconnect() {
    // Wiederverbinden mit dem MQTT-Broker
    while (!client.connected()) {
        Serial.println("MQTT reconnect");
        if (client.connect(mqtt_client_name, mqtt_user, mqtt_password)) {
            Serial.println("connected");
            client.subscribe(topic_sub);
        } else {
            Serial.print("failed: ");
            Serial.println(client.state());
            delay(5000);
        }
    }
}

void schranke_auf() {
    newpos = 90;
    schranken_status = "open";
    is_open = true;
    since_time_opened = millis();
}

void schranke_zu() {
    newpos = 0;
    schranken_status = "closed";
    is_open = false;
}

void schranke_zu_text() {
    display->clear();
    display->setFont(ArialMT_Plain_16);
    display->drawString(0, 0, "Schranke ist zu!");
    display->display();
}