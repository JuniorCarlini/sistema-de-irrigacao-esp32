#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

#define SENSOR_1_PIN 37        // Pino analógico para o sensor 1
#define SENSOR_2_PIN 38        // Pino analógico para o sensor 2
#define DHT_PIN 32             // Pino digital para o DHT22
#define PINO_RELE 25           // Pino digital para controle do relé
#define PINO_LUZ 21            // Pino digital para luz em caso de erro

// Inicializa o DHT
DHT dht(DHT_PIN, DHT22);

// Valores de calibração
int valorMinimo = 4095;    // Valor quando o sensor está no ar (solo seco)
int valorMaximo = 854;     // Valor quando o sensor está completamente na água (solo muito úmido)

// Variáveis de tempo
unsigned long tempoUltimaLeitura = 0;
unsigned long tempoUltimaIrrigacao = 0;
unsigned long tempoUltimaAtualizacaoRele = 0;
const unsigned long intervaloLeitura = 900000;  // 15 minutos (900000 ms)
const unsigned long intervaloIrrigacao = 1000;  // 1 segundo para verificação da irrigação

// Dados de autenticação e conexão
const char* ssid = "-----";
const char* password = "-----";
const String serverDataURL = "https://-----/collect-data/";
const String serverStateURL = "https://-----/collect-solenoid-state/";
const String authKey = "-----"; // Key de autenticação

// Variável para armazenar o estado do relé
bool estadoRele = false;
bool ultimoEstadoRele = false;  // Para verificar mudanças no estado do relé

void setup() {
  Serial.begin(115200);

  // Conectar à rede WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }
  Serial.println("Conectado ao WiFi");

  // Configurar os pinos
  pinMode(SENSOR_1_PIN, INPUT);
  pinMode(SENSOR_2_PIN, INPUT);
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_LUZ, OUTPUT);

  // Inicializa o DHT
  dht.begin();

  // Inicialmente, desliga o relé e a luz de erro
  digitalWrite(PINO_RELE, LOW);
  digitalWrite(PINO_LUZ, LOW);
}

void loop() {
  // Verifica se é hora de ler e enviar os dados (a cada 15 minutos)
  if (millis() - tempoUltimaLeitura >= intervaloLeitura) {
    tempoUltimaLeitura = millis();
    lerDados();
  }

  // Verifica e controla a irrigação
  if (millis() - tempoUltimaIrrigacao >= intervaloIrrigacao) {
    tempoUltimaIrrigacao = millis();
    controlarIrrigacao();
  }

  // Verifica se o estado do relé mudou e envia a atualização
  if (estadoRele != ultimoEstadoRele) {
    ultimoEstadoRele = estadoRele;
    enviarEstadoRele(estadoRele);
  }
}

void lerDados() {
  // Lê os valores dos sensores de umidade do solo
  int umidade1 = analogRead(SENSOR_1_PIN);
  int umidade2 = analogRead(SENSOR_2_PIN);

  // Converte os valores lidos para uma porcentagem
  int umidadePercentual1 = map(umidade1, valorMaximo, valorMinimo, 100, 0);
  int umidadePercentual2 = map(umidade2, valorMaximo, valorMinimo, 100, 0);

  // Limita os valores entre 0% e 100%
  umidadePercentual1 = constrain(umidadePercentual1, 0, 100);
  umidadePercentual2 = constrain(umidadePercentual2, 0, 100);

  // Calcula a média
  int umidadeMedia = (umidadePercentual1 + umidadePercentual2) / 2;

  // Lê a temperatura e umidade do ar
  float umidadeAr = dht.readHumidity();
  float temperaturaAr = dht.readTemperature();

  // Verifica se a leitura falhou
  if (isnan(umidadeAr) || isnan(temperaturaAr)) {
    Serial.println("Falha na leitura do DHT!");
    return;
  }

  // Enviar os dados via HTTP
  enviarDados(temperaturaAr, umidadeAr, umidadeMedia);
}

void controlarIrrigacao() {
  int umidade1 = analogRead(SENSOR_1_PIN);
  int umidade2 = analogRead(SENSOR_2_PIN);

  int umidadePercentual1 = map(umidade1, valorMaximo, valorMinimo, 100, 0);
  int umidadePercentual2 = map(umidade2, valorMaximo, valorMinimo, 100, 0);

  int umidadeMedia = (umidadePercentual1 + umidadePercentual2) / 2;

  // Controlar o estado do relé
  if (umidadeMedia < 10) {
    Serial.println("Iniciando irrigação...");
    digitalWrite(PINO_RELE, HIGH);
    estadoRele = true; // Relé está ligado
  } else if (umidadeMedia >= 20) {
    Serial.println("Desligando irrigação...");
    digitalWrite(PINO_RELE, LOW);
    estadoRele = false; // Relé está desligado
  }
}

void enviarDados(float temperatura, float umidadeAr, float umidadeSolo) {
  if (WiFi.status() == WL_CONNECTED) { // Verifica se está conectado ao WiFi
    HTTPClient http;
    
    http.begin(serverDataURL); // Inicia a conexão
    http.addHeader("Content-Type", "application/json"); // Define o cabeçalho
    http.addHeader("Authorization", authKey); // Autenticação

    // Prepara os dados para enviar
    String jsonData = "{";
    jsonData += "\"temperature\": " + String(temperatura) + ",";
    jsonData += "\"air_humidity\": " + String(umidadeAr) + ",";
    jsonData += "\"soil_humidity\": " + String(umidadeSolo);
    jsonData += "}";

    // Envia os dados e recebe a resposta
    int httpResponseCode = http.POST(jsonData);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Dados enviados com sucesso: " + response);
    } else {
      Serial.println("Falha ao enviar dados. Código de erro: " + String(httpResponseCode));
    }

    http.end(); // Finaliza a conexão
  } else {
    // Se não estiver conectado, acende a luz de erro
    digitalWrite(PINO_LUZ, HIGH);
    Serial.println("Erro: Não conectado à rede!");
  }
}

void enviarEstadoRele(bool estado) {
  if (WiFi.status() == WL_CONNECTED) { // Verifica se está conectado ao WiFi
    HTTPClient http;

    http.begin(serverStateURL); // Inicia a conexão
    http.addHeader("Content-Type", "application/json"); // Define o cabeçalho
    http.addHeader("Authorization", authKey); // Autenticação

    // Prepara os dados do estado do relé para enviar
    String jsonData = "{";
    jsonData += "\"is_open\": \"" + String(estado ? "True" : "False") + "\"";
    jsonData += "}";

    // Envia os dados e recebe a resposta
    int httpResponseCode = http.POST(jsonData);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Estado do relé enviado com sucesso: " + response);
    } else {
      Serial.println("Falha ao enviar estado do relé. Código de erro: " + String(httpResponseCode));
    }

    http.end(); // Finaliza a conexão
  } else {
    // Se não estiver conectado, acende a luz de erro
    digitalWrite(PINO_LUZ, HIGH);
    Serial.println("Erro: Não conectado à rede!");
  }
}
