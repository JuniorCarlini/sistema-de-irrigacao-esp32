#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

#define SENSOR_1_PIN 37        // Pino analógico para o sensor 1
#define SENSOR_2_PIN 38        // Pino analógico para o sensor 2
#define DHT_PIN 32             // Pino digital para o DHT22
#define PINO_RELE 25           // Pino digital para controle do relé
#define PINO_LUZ 21            // Pino digital para luz em caso de erro
#define PINO_RELE_FERTIL 13    // Pino para o controle do relé de irrigação da fértil

// Inicializa o DHT
DHT dht(DHT_PIN, DHT22);

// Tempo de ativação da bomba de agua antes e depois da fertil
int tempoAntesPos = 5000;  // 5 segundo

// Valores de calibração
int valorMinimo = 4050;    // Valor quando o sensor está no ar (solo seco)
int valorMaximo = 500;     // Valor quando o sensor está completamente na água (solo muito úmido)

// Variáveis de tempo
unsigned long tempoUltimaLeitura = 0;
unsigned long tempoUltimaIrrigacao = 0;
unsigned long tempoUltimaAtualizacaoRele = 0;
unsigned long tempoUltimaVerificacaoFertil = 0;
unsigned long tempoReinicio = 0;
const unsigned long intervaloLeitura = 5000;      // 15 minutos (900000 ms)
const unsigned long intervaloIrrigacao = 1000;      // 1 segundo para verificação da irrigação
const unsigned long intervaloVerificacaoFertil = 10000; // 30 minutos (1800000 ms)
const unsigned long intervaloReinicio = 2400000;    // 40 minutos (2400000 ms)

// Dados de autenticação e conexão
const char* ssid = "----";
const char* password = "----";
const String serverDataURL = "http://----/collect-data/";
const String serverStateURL = "http://----/collect-solenoid-state/";
const String serverFertilStateURL = "http://----/data-fertil_state/";
const String serverHistoricoURL = "http://----/historico-fertil/";
const String authKey = "----"; // Key de autenticação

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
    digitalWrite(PINO_LUZ, HIGH);
    delay(1000);
    digitalWrite(PINO_LUZ, LOW);
  }
  Serial.println("Conectado ao WiFi");

  // Configurar os pinos
  pinMode(SENSOR_1_PIN, INPUT);
  pinMode(SENSOR_2_PIN, INPUT);
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_LUZ, OUTPUT);
  pinMode(PINO_RELE_FERTIL, OUTPUT);

  // Inicializa o DHT
  dht.begin();

  // Inicialmente, desliga o relé e a luz de erro
  digitalWrite(PINO_RELE, LOW);
  digitalWrite(PINO_LUZ, LOW);
  digitalWrite(PINO_RELE_FERTIL, LOW);

  // Inicializa o tempo para reinício
  tempoReinicio = millis();
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

  // Verifica a irrigação da fértil a cada 30 minutos
  if (millis() - tempoUltimaVerificacaoFertil >= intervaloVerificacaoFertil) {
    tempoUltimaVerificacaoFertil = millis();
    verificarFertil();
  }

  // Verifica se o estado do relé mudou e envia a atualização
  if (estadoRele != ultimoEstadoRele) {
    ultimoEstadoRele = estadoRele;
    enviarEstadoRele(estadoRele);
  }

  // Reinicia a ESP a cada 40 minutos para evitar travamentos
  if (millis() - tempoReinicio >= intervaloReinicio) {
    Serial.println("Reiniciando ESP para evitar travamentos...");
    ESP.restart();
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

void verificarFertil() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverFertilStateURL);
    http.addHeader("Authorization", authKey);
    
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String response = http.getString();
      // Verifica se a irrigação fértil deve ser iniciada
      if (response.indexOf("\"start_fertil\":true") != -1) {
        // Extrai o tempo de irrigação do JSON
        int tempoIrrigacao = 0;
        int timeStartIndex = response.indexOf("\"time_ferti_ms\":") + 17; // 17 é o comprimento do string `"time_ferti_ms":`
        if (timeStartIndex != -1) {
          int timeEndIndex = response.indexOf(",", timeStartIndex);
          String timeString = response.substring(timeStartIndex, timeEndIndex);
          tempoIrrigacao = timeString.toInt(); // Converte a string para int
        }

        // Ligar o PINO_RELE um tempo antes da irrigação
         // Tempo em milissegundos antes da irrigação
        digitalWrite(PINO_RELE, HIGH);
        enviarEstadoRele(true); // Envia true para o estado do relé antes da irrigação
        delay(tempoAntesPos); // Atraso antes de iniciar a irrigação
        
        // Inicia a irrigação
        Serial.println("Iniciando irrigação da fértil...");
        enviarHistoricoFertil();
        digitalWrite(PINO_RELE_FERTIL, HIGH);
        
        delay(tempoIrrigacao); // Espera durante a irrigação
        
        // Desliga a irrigação
        digitalWrite(PINO_RELE_FERTIL, LOW);
        
        // Desliga o PINO_RELE
        delay(tempoAntesPos); // Atraso após a irrigação
        digitalWrite(PINO_RELE, LOW);
        enviarEstadoRele(false); // Envia false para o estado do relé após a irrigação
        Serial.println("Irrigação da fértil finalizada.");
      }
    } else {
      Serial.println("Falha ao verificar estado da fértil.");
    }
    http.end();
  }
}

void enviarHistoricoFertil() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverHistoricoURL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", authKey);

    String jsonData = "{\"data_fertil\":\"true\"}";

    int httpResponseCode = http.POST(jsonData);
    if (httpResponseCode > 0) {
      Serial.println("Histórico de irrigação enviado com sucesso.");
    } else {
      Serial.println("Falha ao enviar histórico de irrigação.");
    }
    http.end();
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
      Serial.println("Dados dos sensores enviados com sucesso: " + response);
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
      Serial.println("Estado do relé da irrigação enviado com sucesso: " + response);
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
