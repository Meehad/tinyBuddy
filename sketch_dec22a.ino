#include <WiFi.h>
#include <PubSubClient.h>
#include <Seeed_Arduino_SSCMA.h>

// Replace with your WiFi credentials
const char* ssid = "tinyML-2.4G";
const char* password = "tinymlhack";

// MQTT Broker details
const char* mqtt_server = "broker.hivemq.com"; // Replace with your MQTT broker address
const char* mqtt_topic = "home/data"; // MQTT topic to send data

WiFiClient espClient;
PubSubClient client(espClient);

SSCMA AI;

// Flags to track if an increment or decrement message has been sent
bool incrementSent = false;
bool decrementSent = false;

void connectWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("Connected to WiFi!");
}

void connectMQTT() {
    while (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        if (client.connect("ArduinoClient")) {
            Serial.println("Connected to MQTT!");
        } else {
            Serial.print("Failed MQTT connection, retrying...");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(9600);
    AI.begin();

    connectWiFi();
    client.setServer(mqtt_server, 1883);
    connectMQTT();
}

void reconnect() {
    // Loop until we're reconnected to MQTT broker
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ArduinoClient")) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            delay(5000);
        }
    }
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    if (!AI.invoke()) {
        for (int i = 0; i < AI.classes().size(); i++) {
            // Check if confidence score is above 5
            if (AI.classes()[i].score > 5) {
                Serial.print("Class[");
                Serial.print(i);
                Serial.print("] target=");
                Serial.print(AI.classes()[i].target);
                Serial.print(", score=");
                Serial.println(AI.classes()[i].score);

                int currentDetection = AI.classes()[i].target;

                // Handle machine start (driving) and machine stop (shutdown)
                if (currentDetection == 1) { // Sleep detected
                    // If the machine is detected as sleeping, reset decrement flag
                    decrementSent = false;
                    // Send MQTT message to increment active drivers only once
                    if (!incrementSent) {
                        String message = "{\"active_drivers\": 1}"; // Increment active_drivers
                        client.publish(mqtt_topic, message.c_str());
                        incrementSent = true;
                    }
                } else if (currentDetection == 0) { // Awake detected
                    // If the machine is detected as awake, reset increment flag
                    incrementSent = false;
                    // Send MQTT message to decrement active drivers only once
                    if (!decrementSent) {
                        String message = "{\"active_drivers\": -1}"; // Decrement active_drivers
                        client.publish(mqtt_topic, message.c_str());
                        decrementSent = true;
                    }
                }
            }
        }
    }

    delay(1000); // Increased delay to reduce loop frequency
}
