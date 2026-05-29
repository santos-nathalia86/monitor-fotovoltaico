# 🌞 Monitor Inteligente de Eficiência Fotovoltaica

> Protótipo IoT desenvolvido com ESP32 para monitoramento em tempo real de painéis solares, com exibição local via LCD, controle de atuador (servo motor) e comunicação remota via protocolo MQTT sobre TCP/IP.

🔗 **Simulação online (Wokwi):** https://wokwi.com/projects/459222474259671041

---

## 📋 Sumário

- [Descrição do Projeto](#-descrição-do-projeto)
- [Como Reproduzir](#-como-reproduzir)
- [Software e Documentação de Código](#-software-e-documentação-de-código)
- [Hardware Utilizado](#-hardware-utilizado)
- [Comunicação e Protocolos](#-comunicação-e-protocolos)
- [Tópicos MQTT](#-tópicos-mqtt)
- [Estrutura do Repositório](#-estrutura-do-repositório)

---

## 📖 Descrição do Projeto

O **Monitor Inteligente de Eficiência Fotovoltaica** é um sistema IoT embarcado que simula o monitoramento de um painel solar fotovoltaico. O sistema:

- Lê a irradiação solar simulada via **potenciômetro** (sensor analógico)
- Calcula a **potência gerada** (0–300 W) e a **autonomia estimada** do veículo (0–15 km)
- Exibe os dados em tempo real em um **display LCD 16×2**
- Posiciona um **servo motor** proporcionalmente à potência lida (0°–180°), simulando um plugue de carregamento
- Publica os dados via **MQTT** no broker público HiveMQ a cada 3 segundos
- Recebe **comandos remotos** via MQTT para ligar/desligar o servo motor
- Mede e reporta os **tempos de resposta** do sensor e do atuador via Monitor Serial

---

## 🚀 Como Reproduzir

### Opção 1 — Simulação no Wokwi (recomendado)

1. Acesse: https://wokwi.com/projects/459222474259671041
2. Clique em **▶ Start Simulation**
3. Gire o potenciômetro para simular variações de irradiação solar
4. Observe o LCD atualizar os valores de potência e autonomia
5. Abra o **Monitor Serial** para ver os dados MQTT e os tempos de resposta

### Opção 2 — Hardware físico

**Pré-requisitos:**
- [Arduino IDE](https://www.arduino.cc/en/software) instalado
- Placa ESP32 configurada na IDE ([tutorial](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html))
- Bibliotecas instaladas (ver seção [Hardware](#-hardware-utilizado))

**Passos:**
1. Clone este repositório:
   ```bash
   git clone https://github.com/SEU_USUARIO/monitor-fotovoltaico.git
   ```
2. Abra o arquivo `src/sketch.ino` na Arduino IDE
3. Ajuste as credenciais WiFi se necessário (por padrão usa rede aberta):
   ```cpp
   const char* ssid = "SUA_REDE_WIFI";
   const char* password = "SUA_SENHA";
   ```
4. Conecte os componentes conforme o [diagrama de hardware](#esquema-de-conexões)
5. Faça o upload para a placa ESP32
6. Abra o Monitor Serial (115200 baud) para acompanhar os logs

### Monitorando via MQTT

Para visualizar os dados publicados em tempo real, acesse o cliente WebSocket do HiveMQ:

1. Acesse: https://www.hivemq.com/demos/websocket-client/
2. Conecte ao broker: `broker.hivemq.com`, porta `8000`
3. Inscreva-se no tópico: `monitor/solar/dados`

Para enviar comandos remotos ao servo, publique no tópico `monitor/solar/comando`:
- `ligar` → servo vai para 90°
- `desligar` → servo vai para 0°

---

## 💻 Software e Documentação de Código

### Arquivo principal: `src/sketch.ino`

```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ── Configurações WiFi ──────────────────────────────────────────────────────
const char* ssid     = "Wokwi-GUEST";   // SSID da rede (Wokwi usa rede aberta)
const char* password = "";               // Senha (vazia para redes abertas)

// ── Configurações MQTT ──────────────────────────────────────────────────────
const char* mqtt_server = "broker.hivemq.com";  // Broker público HiveMQ
WiFiClient   espClient;
PubSubClient client(espClient);

// ── Periféricos ─────────────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD I2C: endereço 0x27, 16 colunas, 2 linhas
Servo meuServo;                        // Servo motor no pino GPIO 18
int posAtual = -1;                     // Posição atual do servo (-1 = não inicializado)
```

### Fluxo do programa

```
┌─────────────────────────────────────────────────────────────────┐
│                            setup()                              │
│  1. Inicializa Serial (115200 baud)                             │
│  2. Conecta ao WiFi (setup_wifi)                                │
│  3. Configura servidor e callback MQTT                          │
│  4. Inicializa LCD com backlight                                │
│  5. Anexa servo ao GPIO 18 (pulso: 500–2400 µs)                 │
│  6. Teste inicial do servo: 0° → 90° → aguarda loop             │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                            loop()                               │
│  1. Verifica/reconecta MQTT se desconectado                     │
│  2. Processa mensagens recebidas (client.loop)                  │
│  3. Lê sensor analógico: analogRead(GPIO 34)                    │
│  4. Calcula potência: (leitura / 4095) × 300 W                  │
│  5. Calcula autonomia: potência × 0,05 km                       │
│  6. Atualiza LCD (linha 0: potência | linha 1: autonomia)       │
│  7. Mapeia potência → ângulo do servo (0–300 W → 0°–180°)       │
│  8. Aciona servo + mede tempo de resposta do ATUADOR            │
│  9. Publica JSON no tópico MQTT + mede tempo de resposta SENSOR │
│  10. Aguarda 3 segundos (delay 3000 ms)                         │
└─────────────────────────────────────────────────────────────────┘
```

### Funções

| Função | Descrição |
|---|---|
| `setup_wifi()` | Conecta ao WiFi e aguarda `WL_CONNECTED` com polling de 500 ms |
| `callback(topic, payload, length)` | Processa mensagens recebidas: `"ligar"` → servo 90° / `"desligar"` → servo 0° |
| `reconnect()` | Reestabelece conexão MQTT com ID aleatório `ESP32_Solar_XXXX`; subscreve `monitor/solar/comando` |
| `setup()` | Inicialização de todos os periféricos e conexões |
| `loop()` | Ciclo principal de leitura, processamento, exibição e publicação (período: 3 s) |

### Cálculos principais

```cpp
// Leitura ADC de 12 bits → Potência em Watts
float potenciaWatts = (valorAnalogico / 4095.0) * 300.0;

// Potência → Autonomia estimada (km)
float autonomia = potenciaWatts * 0.05;

// Potência → Ângulo do servo (0° a 180°)
int angulo = map(potenciaWatts, 0, 300, 0, 180);
```

### Medição de tempos de resposta

```cpp
// Tempo de resposta do ATUADOR (servo motor)
unsigned long inicioAtuador = millis();
meuServo.write(angulo);
unsigned long tempoAtuador = millis() - inicioAtuador;

// Tempo de resposta do SENSOR (publicação MQTT)
unsigned long inicioSensor = millis();
client.publish("monitor/solar/dados", msg);
unsigned long tempoSensor = millis() - inicioSensor;
```

### Bibliotecas utilizadas

| Biblioteca | Versão | Finalidade |
|---|---|---|
| `WiFi.h` | (built-in ESP32) | Conexão WiFi TCP/IP |
| `PubSubClient` | 2.8+ | Cliente MQTT |
| `LiquidCrystal_I2C` | 1.1.2+ | Controle do display LCD via I2C |
| `ESP32Servo` | 0.13+ | Controle do servo motor no ESP32 |

---

## 🔧 Hardware Utilizado

### Plataforma de Desenvolvimento

| Componente | Especificação |
|---|---|
| **Microcontrolador** | ESP32 WROOM-32 |
| **Arquitetura** | Xtensa LX6 dual-core 240 MHz |
| **Memória Flash** | 4 MB |
| **RAM** | 520 KB SRAM |
| **WiFi** | 802.11 b/g/n (2,4 GHz) |
| **ADC** | 12 bits (0–4095), GPIO 34 |
| **GPIO utilizado** | 18 (Servo), 34 (ADC), 21/22 (I2C) |
| **Alimentação** | 3,3 V / 5 V via USB |
| **Simulador** | Wokwi (https://wokwi.com) |

### Sensores

| Componente | Tipo | Conexão | Função |
|---|---|---|---|
| **Potenciômetro** | Sensor analógico resistivo | GPIO 34 (ADC1_CH6) | Simula irradiação solar (0–300 W) |

> **Nota:** O potenciômetro substitui o sensor de irradiação solar real (ex.: fotodiodo ou célula de referência). Em uma implementação física, pode ser substituído por um sensor BH1750 (I2C) ou um divisor de tensão com LDR.

### Atuadores

| Componente | Modelo | Conexão | Função |
|---|---|---|---|
| **Servo Motor** | SG90 (compatível) | GPIO 18, PWM (500–2400 µs) | Simula abertura/fechamento do plugue de carregamento |

**Configuração do servo:**
```cpp
meuServo.attach(18, 500, 2400);  // pino, pulso mínimo (µs), pulso máximo (µs)
```
- `0°` → veículo desconectado (sem geração suficiente)
- `90°` → comando remoto "ligar" via MQTT
- `0°–180°` → proporcional à potência solar lida

### Display

| Componente | Modelo | Conexão | Função |
|---|---|---|---|
| **LCD** | 16×2 I2C | SDA: GPIO 21 / SCL: GPIO 22 | Exibição local de potência e autonomia |
| **Endereço I2C** | 0x27 | — | Endereço padrão do módulo PCF8574 |

### Esquema de Conexões

```
ESP32          Potenciômetro
GPIO 34   ←── Pino central (sinal)
3.3V      ──► Pino esquerdo (VCC)
GND       ──► Pino direito (GND)

ESP32          Servo Motor SG90
GPIO 18   ──► Sinal (laranja/amarelo)
5V        ──► VCC (vermelho)
GND       ──► GND (marrom/preto)

ESP32          LCD I2C 16x2
GPIO 21   ──► SDA
GPIO 22   ──► SCL
5V        ──► VCC
GND       ──► GND
```

### Diagrama JSON (Wokwi) — `diagram.json`

O arquivo `diagram.json` descreve todas as conexões dos componentes na plataforma Wokwi e pode ser importado diretamente em https://wokwi.com para reproduzir o circuito completo.

---

## 📡 Comunicação e Protocolos

### Arquitetura de Comunicação

```
┌─────────────┐    WiFi/TCP     ┌──────────────────┐    MQTT      ┌────────────────┐
│    ESP32    │ ──────────────► │  broker.hivemq   │ ──────────► │ Cliente MQTT   │
│  (Publisher)│                 │  .com : 1883     │             │ (Assinante)    │
│             │ ◄────────────── │                  │ ◄────────── │                │
│  (Subscriber│    TCP/IP       └──────────────────┘    MQTT     └────────────────┘
└─────────────┘
```

### Protocolo MQTT

| Parâmetro | Valor |
|---|---|
| **Broker** | `broker.hivemq.com` |
| **Porta** | `1883` (TCP) / `8000` (WebSocket) |
| **Versão MQTT** | 3.1.1 |
| **QoS** | 0 (at most once) |
| **Keep-alive** | 15 s (padrão PubSubClient) |
| **Client ID** | `ESP32_Solar_XXXX` (gerado aleatoriamente) |
| **Autenticação** | Nenhuma (broker público) |

### Tópicos MQTT

#### 📤 Publicação — `monitor/solar/dados`
Publicado pelo ESP32 a cada **3 segundos** com os dados do sensor.

**Formato do payload (JSON):**
```json
{
  "potencia": 202.3,
  "autonomia": 10.1
}
```

| Campo | Tipo | Unidade | Descrição |
|---|---|---|---|
| `potencia` | float (1 casa) | Watts (W) | Potência solar calculada |
| `autonomia` | float (1 casa) | km | Autonomia estimada do veículo |

#### 📥 Subscrição — `monitor/solar/comando`
Recebido pelo ESP32 para controle remoto do servo.

| Payload | Ação | Posição do servo |
|---|---|---|
| `ligar` | Conecta plugue de carregamento | 90° |
| `desligar` | Desconecta plugue de carregamento | 0° |

### Camada de Transporte (TCP/IP)

- **Protocolo:** TCP/IP sobre WiFi 802.11 b/g/n
- **Camada de aplicação:** MQTT 3.1.1 sobre TCP
- **Rede WiFi:** `Wokwi-GUEST` (aberta, simulada) / qualquer rede 2,4 GHz em hardware real
- **Reconexão automática:** implementada em `reconnect()` com retry a cada 5 segundos

### Interface I2C (LCD)

| Parâmetro | Valor |
|---|---|
| **Protocolo** | I2C (TWI) |
| **Velocidade** | 100 kHz (standard mode) |
| **Endereço** | 0x27 |
| **Pinos** | SDA: GPIO 21 / SCL: GPIO 22 |

### Interface PWM (Servo)

| Parâmetro | Valor |
|---|---|
| **Protocolo** | PWM (Pulse Width Modulation) |
| **Frequência** | ~50 Hz |
| **Pulso mínimo** | 500 µs (0°) |
| **Pulso máximo** | 2400 µs (180°) |
| **Pino** | GPIO 18 |

---

## 📁 Estrutura do Repositório

```
monitor-fotovoltaico/
│
├── README.md                  # Este arquivo
│
├── src/
│   └── sketch.ino             # Código-fonte principal (Arduino/ESP32)
│
├── hardware/
│   ├── diagram.json           # Diagrama de circuito (Wokwi)
│   ├── libraries.txt          # Lista de bibliotecas necessárias
│   └── schematic.png          # Esquema de conexões (imagem)
│
└── docs/
    └── artigo.pdf             # Artigo científico completo do projeto
```

---

## 📊 Resultados Obtidos

| Métrica | Valor médio |
|---|---|
| Tempo de resposta do sensor (MQTT publish) | ~2–3 ms |
| Tempo de resposta do atuador (servo write) | ~0 ms |
| Intervalo de publicação MQTT | 3000 ms |
| Faixa de potência simulada | 0–300 W |
| Faixa de autonomia calculada | 0–15 km |

---

## 👥 Autores

Desenvolvido como projeto de IoT — ESP32 com comunicação MQTT e monitoramento fotovoltaico.
Por Filipe Versani Oliveira, Marcelly Fonseca da Cruz e Nathalia Santos Lima

---

## 📄 Licença

Este projeto está licenciado sob a [MIT License](LICENSE).
