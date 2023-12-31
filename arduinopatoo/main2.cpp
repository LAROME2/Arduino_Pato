// NN3002B.502
// Pato: ESP32 con SIM800L integrada
// Cálculo de temperatura con envío de datos
// 14 de octubre de 2023

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

void setup_wifi(); // Función para conectarse a Wi-Fi
void callback(char*, byte*, unsigned int); // Función para procesar mensajes recibidos por MQTT
void reconnect(); // Función para conectarse a MQTT
float readTemp(); // Función para leer la temperatura

const int ThermistorPin = 35;         // Pin de datos del termistor (pines 2, 4 no funcionan por uso de Wi-Fi)
const int SeriesResistor = 10000;     // Resistencia en serie con el termistor [ohms]
const int ThermistorNominal = 10000;  // Resistencia del termistor a la temperatura nominal [ohms]
const int NominalTemp = 25;           // Temperatura para resistencia nominal del termistor [ºC]
const int BCoefficient = 3950;        // Coeficiente beta del termistor [ºC]
const int NumSamples = 20;            // Número de muestras para sacar un promedio de lecturas
int samples[NumSamples];              // Arreglo con las muestras

const char* ssid = "Totalplay-4EA5";
const char* password =  "4EA54C27Ukm4kysW";
const char* mqtt_server = "broker.mqtt-dashboard.com";
const char* topico_salida = "equipoPATO";
const char* topico_entrada = "equipoPATO";

const int MSG_BUFFER_SIZE = 80;       // Tamaño de buffer del mensaje
char msg[MSG_BUFFER_SIZE];            // Generar un arreglo para el mensaje

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  if (!client.connected()) {
    reconnect();
  }
}

void loop() {
  float t;
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  t = readTemp();
  snprintf (msg, MSG_BUFFER_SIZE, "{\"dispositivo\":\"Refrigerador 1\",\"tipo\":\"Temperatura\",\"dato\":%.2f}", t*1.0);
  client.publish(topico_entrada, msg); 

  delay(5000);
}

void setup_wifi() {
  WiFi.begin(ssid, password);
    Serial.print("\nConnecting");

    while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(100);
    }
    Serial.print("\nConnected to the WiFi network ");
    Serial.println(ssid);
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() { // Función para conectarse a MQTT
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection... ");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected.");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      //client.subscribe("inTopic");
      client.subscribe(topico_entrada);
      client.subscribe(topico_salida);   
    } else {
      Serial.print("Failed, rc = ");
      Serial.print(client.state());
      Serial.println(". Try again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

float readTemp() { // Función para leer la temperatura del termistor
  uint8_t i; // Definición de contador (entero de un byte sin signo).
  float average;
 
  // Tomar N muestras y guardarlas en el arreglo con un pequeño delay
  for (i = 0; i < NumSamples; i++) {
   samples[i] = analogRead(ThermistorPin);
   delay(10);
  }
 
  // Promediar las muestras
  average = 0;
  for (i = 0; i < NumSamples; i++) {
     average += samples[i];
  }
  average /= NumSamples;
 
  Serial.print("Average analog reading "); 
  Serial.println(average);

  // Convertir el valor análogo a una resistencia
  average = (4095 / average)  - 1;     // (4095 / ADC - 1). 4095 por tener un ADC de 12 bits (0-4095).
  average = SeriesResistor / average;  // 10K / (4095 / ADC - 1)
  Serial.print("Thermistor resistance "); 
  Serial.print(average);
  Serial.println(" ohms");
  
  // Utilizar la ecuación simplificada del parámetro B para un termistor: 1/T = 1/T_0 + (1/B) * ln(R/R_0)
    // T = temperatura medida [K]
    // T_0 = temperatura nominal absoluta [K]
    // B = parámetro beta del termistor [K]
    // R = resistencia medida [ohms]
    // R_0 = resistencia a la temperatura nominal [ohms]
  float steinhart;
  steinhart = average / ThermistorNominal;        // R/R_0
  steinhart = log(steinhart);                     // ln(R/R_0)
  steinhart /= BCoefficient;                      // 1/B * ln(R/R_0)
  steinhart += 1.0 / (NominalTemp + 273.15);      // + (1/T_0)
  steinhart = 1.0 / steinhart;                    // Invertir
  steinhart -= 273.15;                            // Convertir temperatura absoluta [K] a Celsius [ºC]
  
  Serial.print("Temperature "); 
  Serial.print(steinhart);
  Serial.println(" ºC");

  return steinhart;
}
