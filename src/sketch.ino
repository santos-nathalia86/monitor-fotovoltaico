#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ---------------- WIFI ----------------
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ---------------- MQTT ----------------
const char* mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient client(espClient);

// ---------------- LCD E SERVO ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo meuServo;

int posAtual = -1;

// ---------------- WIFI ----------------
void setup_wifi() {
  Serial.println("Conectando ao WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
}

// ---------------- MQTT CALLBACK ----------------
void callback(char* topic, byte* payload, unsigned int length) {
  String mensagem;

  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  Serial.print("Recebido: ");
  Serial.println(mensagem);

  // Controle remoto do servo
  if (mensagem == "ligar") {
    meuServo.write(90);
    posAtual = 90;
  } else if (mensagem == "desligar") {
    meuServo.write(0);
    posAtual = 0;
  }
}

// ---------------- MQTT ----------------
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");

    String clientId = "ESP32_Solar_";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" conectado!");
      client.subscribe("monitor/solar/comando");
    } else {
      Serial.print(" erro=");
      Serial.print(client.state());
      Serial.println(" tentando novamente...");
      delay(5000);
    }
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  lcd.init();
  lcd.backlight();

  //SERVO AJUSTADO
  meuServo.attach(18, 500, 2400);

  //TESTE INICIAL DO SERVO
  meuServo.write(0);
  delay(1000);
  meuServo.write(90);
  delay(1000);

  randomSeed(analogRead(0));
}

// ---------------- LOOP ----------------
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // -------- SENSOR --------
  int valorAnalogico = analogRead(34);

  float potenciaWatts = (valorAnalogico / 4095.0) * 300.0;
  float autonomia = potenciaWatts * 0.05;

  // -------- LCD --------
  lcd.setCursor(0, 0);
  lcd.print("Solar: ");
  lcd.print(potenciaWatts, 1);
  lcd.print("W   ");

  lcd.setCursor(0, 1);
  lcd.print("Auton: ");
  lcd.print(autonomia, 1);
  lcd.print("km  ");

  // -------- SERVO --------
  int angulo = map(potenciaWatts, 0, 300, 0, 180);

  //Medindo o tempo médio de resposta do atuador (servo motor)
  unsigned long inicioAtuador = millis();

  meuServo.write(angulo);

  unsigned long fimAtuador = millis();

  unsigned long tempoAtuador = fimAtuador - inicioAtuador;

  Serial.print("Tempo resposta ATUADOR: ");
  Serial.print(tempoAtuador);
  Serial.println(" ms");

  // -------- MQTT --------
  char msg[100];
  snprintf(msg, 100, "{\"potencia\": %.1f, \"autonomia\": %.1f}", potenciaWatts, autonomia);

  unsigned long inicioSensor = millis();

  //Medindo o tepo médio de resposta do sensor (potênciometro)
  client.publish("monitor/solar/dados", msg);

  unsigned long fimSensor = millis();

  unsigned long tempoSensor = fimSensor - inicioSensor;

  Serial.print("Tempo resposta SENSOR: ");
  Serial.print(tempoSensor);
  Serial.println(" ms");

  Serial.print("Enviado: ");
  Serial.println(msg);

  delay(3000);
}