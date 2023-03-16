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

//#include <SPI.h>      // für SPI-Schnittstelle
#include <MFRC522v2.h>  // für den Kartenleser
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>

#define DEBUG false  // auf true setzen für Debug-Ausgaben

// Einstellungen für Kartenleser
MFRC522DriverPinSimple ss_pin(D4);
SPIClass &spiClass = SPI;
const SPISettings spiSettings = SPISettings(SPI_CLOCK_DIV4, MSBFIRST, SPI_MODE0);
MFRC522DriverSPI driver{ss_pin, spiClass, spiSettings};
MFRC522 mfrc522{driver};


//byte cardUid[4];  // zum Einlesen der Karten-UID
//bool readSuccessful = false;
//int idleCount = 0;

const char validCards[1][4] = {  // Datenstruktur mit gültigen Karten-IDs
        {0xF3, 0xDA, 0x1E, 0x19}
};
const char nrOfValidCards = sizeof(validCards) / sizeof(validCards[0]);  // Anzahl gültiger Karten
const unsigned long sameCardInterval = 5000;  // Intervall in dem die gleiche Karte nicht nochmal behandelt werden soll
char lastCard[4];  // Variable für die zuletzt gelesene Karten-ID
unsigned long lastCardMillis = 0;  // Variable zur Speicherung des Zeitstempels für das sameCardInterval


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
    Serial.println("Setup abgeschlossen.");
}


/**
 * loop
 */
void loop() {
    // falls keine Karte erkannt wird oder gelesen werden kann, dann von vorne
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return;
    } else  // weiter falls Karte gelesen
    {
        char thisCard[4];  // Variable für die Karten-UID
        bool sameAsLastCard = true;  // Variable um zu speichern, ob das eine neue Karte ist


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

            // alle gültigen Karten durchgehen
            for (int i = 0; i < nrOfValidCards; i++) {
                bool matchesThisValidCard = true;  // Variable um zu speichern, ob dieser gültigen entspricht
                // Karten byteweise vergleichen
                for (int j = 0; j < 4; j++) {
                    if (thisCard[j] != validCards[i][j]) {
                        matchesThisValidCard = false;  // merken, wenn ein Unterschied erkannt wurde
                    }
                }
                if (matchesThisValidCard) {
                    matchesSomeValidCard = true;  // merken, dass diese Karte einer gültigen Karte entspricht
                }
            }

            // Erkennen einer gültigen oder ungültigen Karte ausgeben
            if (matchesSomeValidCard) {
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

        }  // Ende neue Karte
    }  // Ende Karte gelesen


    if (DEBUG) {
        MFRC522Debug::PICC_DumpToSerial(mfrc522, Serial, &(mfrc522.uid));
    }

    delay(100);
}