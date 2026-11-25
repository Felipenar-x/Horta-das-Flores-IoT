/*
 * Projeto: Monitoramento de Luminosidade na Horta das Flores
 * Grupo: Os Felipes (Felipe Yuji, Leonardo Moreno, Gabriel da Silva)
 * Disciplina: Desenvolvimento de Sistemas de IoT - Mackenzie (2026)
 * Versao: firmware original + estimativa de iluminancia em lux
 * As credenciais locais ficam em secrets.h.
 */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <math.h>

// --- CONFIGURACOES ORIGINAIS DO SENSOR ---
#define LIGHT_SENSOR_PIN 34
const int ADC_ESCURO = 100;
const int ADC_CLARO = 3800;

// --- NOVO: MODELO PROVISORIO PARA ESTIMATIVA DE LUX ---
// Pressupoe: 3V3 -- LDR -- GPIO34 -- resistor fixo -- GND.
// CONFIRME o resistor e calibre o LDR antes de tratar lux como medida.
const float VCC_LDR = 3.3f;
const float R_FIXO = 10000.0f;  // ohms: valor presumido, conferir montagem
const float R10 = 15000.0f;     // ohms a 10 lx: hipotese para GL5528
const float GAMMA = 0.7f;       // hipotese para GL5528
// Faixa nominal util do ADC do ESP32 classico com atenuacao de 11 dB.
const float ADC_MV_MIN = 150.0f;
const float ADC_MV_MAX = 3100.0f;

// --- CONFIGURACOES ORIGINAIS DO WI-FI E MQTT ---
#include "secrets.h"
const char* mqtt_server = "d9218f78.ala.us-east-1.emqxsl.com";
const int mqtt_port = 8883;
const char* topico = "hortadasflores/osfelipes/esp32-01/lumi" "nosidade";

WiFiClientSecure espClient;
PubSubClient client(espClient);
unsigned long e_tempoAnterior = 0;
const long intervalo = 10000;
int sequencia = 1;

// --- NOVO: CONVERSAO DE TENSAO PARA LUX ESTIMADO ---
float calcularLux(float tensaoMv, int adcMedia) {
  // Nao inventar uma leitura de lux quando o ADC esta fora da faixa util.
  if (tensaoMv < ADC_MV_MIN || tensaoMv >= ADC_MV_MAX || adcMedia >= 4095) {
    return NAN;
  }
  float tensao = tensaoMv / 1000.0f;
  if (tensao <= 0.0f || tensao >= VCC_LDR) return NAN;
  float resistenciaLDR = R_FIXO * (VCC_LDR / tensao - 1.0f);
  if (resistenciaLDR <= 0.0f) return NAN;
  float lux = 10.0f * powf(R10 / resistenciaLDR, 1.0f / GAMMA);
  return isfinite(lux) ? lux : NAN;
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("Endereco IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Tentando conexao MQTT...");
    String clientId = "ESP32Horta-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("conectado!");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);
  setup_wifi();

  // Mantido o modo que funcionou no firmware original.
  // TLS criptografa, mas NAO verifica a identidade do servidor.
  // Substituir por setCACert() com a CA valida quando disponivel.
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  // NOVO: espaco para o JSON maior, incluindo topico e cabecalho MQTT.
  if (!client.setBufferSize(512)) {
    Serial.println("Falha ao ampliar o buffer MQTT");
  }

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Aguardando sincronizacao NTP...");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(100);
    now = time(nullptr);
  }
  Serial.println("NTP sincronizado!");
  Serial.println("--- Sistema Horta das Flores: LDR + lux estimado ---");
  Serial.println("AVISO: lux ainda nao calibrado; TLS sem verificacao da CA.");
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  unsigned long tempoAtual = millis();
  if (tempoAtual - e_tempoAnterior >= intervalo) {
    e_tempoAnterior = tempoAtual;

    // Filtragem original: 20 amostras com intervalo de 10 ms.
    // NOVO: leituras adicionais de tensao calibrada do ADC.
    long somaAmostras = 0;
    unsigned long somaMilivolts = 0;
    for (int i = 0; i < 20; i++) {
      somaAmostras += analogRead(LIGHT_SENSOR_PIN);
      somaMilivolts += analogReadMilliVolts(LIGHT_SENSOR_PIN);
      delay(10);
    }
    int adcMedia = somaAmostras / 20;
    float tensaoMediaMv = somaMilivolts / 20.0f;

    // Escala relativa original, sem alteracao na formula ou nos extremos.
    float luzPct = 100.0 * (adcMedia - ADC_ESCURO) / (ADC_CLARO - ADC_ESCURO);
    if (luzPct < 0.0) luzPct = 0.0;
    if (luzPct > 100.0) luzPct = 100.0;

    // NOVO: estimativa independente da porcentagem relativa.
    float luxEstimado = calcularLux(tensaoMediaMv, adcMedia);
    bool luxValido = isfinite(luxEstimado);

    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char timeString[25];
    strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

    // JSON: os seis campos originais continuam identicos.
    String payload = "{";
    payload += "\"device_id\":\"esp32-01\",";
    payload += "\"sensor\":\"LDR_5mm\",";
    payload += "\"timestamp\":\"" + String(timeString) + "\",";
    payload += "\"seq\":" + String(sequencia) + ",";
    payload += "\"adc_media\":" + String(adcMedia) + ",";
    payload += "\"lumi" "nosidade_relativa_pct\":" + String(luzPct, 1);
    // Campos novos; null e um valor JSON valido para leitura indisponivel.
    payload += ",\"lumi" "nosidade_lux_estimada\":";
    if (luxValido) payload += String(luxEstimado, 2);
    else payload += "null";
    payload += ",\"unidade_lux\":\"lx\",";
    payload += "\"lux_status\":\"";
    payload += luxValido ? "estimativa_nao_calibrada" : "fora_faixa_adc";
    payload += "\"}";

    Serial.println("Publicando no EMQX: ");
    Serial.println(payload);
    if (client.publish(topico, payload.c_str(), false)) {
      Serial.println("Mensagem publicada com sucesso!");
    } else {
      Serial.println("Falha ao publicar mensagem.");
    }
    Serial.println("----------------------------------------");
    sequencia++;
  }
}
