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

// Parâmetros de tempo e calibração
const int tempoAntesPos = 5000;    // Tempo antes e depois da irrigação fértil
const int valorMinimo = 4050;      // Valor quando o solo está seco
const int valorMaximo = 500;       // Valor quando o solo está muito úmido

// Variáveis de tempo e configuração
unsigned long tempoUltimaLeitura = 0;
unsigned long tempoUltimaIrrigacao = 0;
unsigned long tempoUltimaAtualizacaoRele = 0;
unsigned long tempoUltimaVerificacaoFertil = 0;
unsigned long tempoReinicio = 0;

const unsigned long intervaloLeitura = 150000;          // Intervalo de leitura (10 segundos)
const unsigned long intervaloIrrigacao = 1000;         // Intervalo de irrigação (1 segundo)
const unsigned long intervaloVerificacaoFertil = 300000; // Intervalo de verificação da fértil (30 minutos)
const unsigned long intervaloReinicio = 2400000;       // Intervalo de reinício (40 minutos)

// Dados de autenticação e conexão
const char* ssid = "----";
const char* password = "----";
const String serverDataURL = "http://----/collect-data/";
const String serverStateURL = "http://----/collect-solenoid-state/";
const String serverFertilStateURL = "http://----/data-fertil_state/";
const String serverHistoricoURL = "http://----/historico-fertil/";
const String authKey = "----"; // Key de autenticação

bool estadoRele = false;
bool ultimoEstadoRele = false;  // Para verificar mudanças no estado do relé

void setup() {
  Serial.begin(9600);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Conectando ao WiFi...");
    digitalWrite(PINO_LUZ, HIGH);
    delay(1000);
  }
  Serial.println("Conectado ao WiFi");
  digitalWrite(PINO_LUZ, LOW);

  pinMode(SENSOR_1_PIN, INPUT);
  pinMode(SENSOR_2_PIN, INPUT);
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_LUZ, OUTPUT);
  pinMode(PINO_RELE_FERTIL, OUTPUT);

  dht.begin();
  digitalWrite(PINO_RELE, LOW);
  digitalWrite(PINO_LUZ, LOW);
  digitalWrite(PINO_RELE_FERTIL, LOW);

  tempoReinicio = millis();
  Serial.println("tempoReinicio");
  Serial.println(tempoReinicio);
}

void loop() {
  unsigned long tempoAtual = millis();
  Serial.println("tempoAtual");
  Serial.println(tempoAtual);

  if ((tempoAtual - tempoUltimaLeitura) >= intervaloLeitura) {
    tempoUltimaLeitura = tempoAtual;
    lerDados();
  }

  if ((tempoAtual - tempoUltimaIrrigacao) >= intervaloIrrigacao) {
    controlarIrrigacao();
  }

  if ((tempoAtual - tempoUltimaVerificacaoFertil) >= intervaloVerificacaoFertil) {
    tempoUltimaVerificacaoFertil = tempoAtual;
    verificarFertil();
  }

  if (estadoRele != ultimoEstadoRele) {
    ultimoEstadoRele = estadoRele;
    enviarEstadoRele(estadoRele);
  }

  if (tempoAtual >= intervaloReinicio) {
    Serial.println("Reiniciando ESP para evitar travamentos...");
    ESP.restart();
  }

  delay(1000); // Aguarda 1 segundo antes da próxima verificação
}

void lerDados() {
  int umidade1 = analogRead(SENSOR_1_PIN);
  int umidade2 = analogRead(SENSOR_2_PIN);

  int umidadePercentual1 = map(umidade1, valorMaximo, valorMinimo, 100, 0);
  int umidadePercentual2 = map(umidade2, valorMaximo, valorMinimo, 100, 0);

  umidadePercentual1 = constrain(umidadePercentual1, 0, 100);
  umidadePercentual2 = constrain(umidadePercentual2, 0, 100);
  int umidadeMedia = (umidadePercentual1 + umidadePercentual2) / 2;

  float umidadeAr = dht.readHumidity();
  float temperaturaAr = dht.readTemperature();

  if (isnan(umidadeAr) || isnan(temperaturaAr)) {
    Serial.println("Falha na leitura do DHT!");
    return;
  }

  enviarDados(temperaturaAr, umidadeAr, umidadeMedia);
}

void controlarIrrigacao() {
  int umidade1 = analogRead(SENSOR_1_PIN);
  int umidade2 = analogRead(SENSOR_2_PIN);

  int umidadePercentual1 = map(umidade1, valorMaximo, valorMinimo, 100, 0);
  int umidadePercentual2 = map(umidade2, valorMaximo, valorMinimo, 100, 0);
  int umidadeMedia = (umidadePercentual1 + umidadePercentual2) / 2;

  if (umidadeMedia < 60 && !estadoRele) {
    tempoUltimaIrrigacao = millis();
    Serial.println("Iniciando irrigação...");
    enviarEstadoRele(true); // Envia true para o estado do relé antes da irrigação
    digitalWrite(PINO_RELE, HIGH);
    estadoRele = true;

    while (umidadeMedia < 85) {
      umidade1 = analogRead(SENSOR_1_PIN);
      umidade2 = analogRead(SENSOR_2_PIN);

      umidadePercentual1 = map(umidade1, valorMaximo, valorMinimo, 100, 0);
      umidadePercentual2 = map(umidade2, valorMaximo, valorMinimo, 100, 0);
      umidadeMedia = (umidadePercentual1 + umidadePercentual2) / 2;

      Serial.println("Aguardando para desligar irrigacao...");
      Serial.println(umidadeMedia);
      delay(1000);
    }

    Serial.println("Desligando irrigação...");
    digitalWrite(PINO_RELE, LOW);
    enviarEstadoRele(false); // Envia false para o estado do relé antes da irrigação
    estadoRele = false;
  }
}

void verificarFertil() {
  if (WiFi.status() == WL_CONNECTED) { // Verifica se está conectado ao Wi-Fi
    HTTPClient http;

    // Configura o URL do servidor
    http.begin(serverFertilStateURL);

    // Configura o cabeçalho de autenticação
    http.addHeader("Authorization", authKey);

    // Faz a requisição GET
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String response = http.getString();
      Serial.println("Resposta do servidor: " + response); // Imprime a resposta completa

      // Extrai os valores "start_fertil" e "time_ferti_ms"
      int tempoIrrigacao = 0;
      bool startFertil = false;

      // Parse da resposta para "start_fertil"
      int startPos = response.indexOf("\"start_fertil\":") + 15;
      int endPos = response.indexOf(",", startPos);
      String startFertilStr = response.substring(startPos, endPos);
      startFertil = (startFertilStr == "true");

      // Parse da resposta para "time_ferti_ms"
      startPos = response.indexOf("\"time_ferti_ms\":") + 16;
      endPos = response.indexOf("}", startPos);
      String timeFertiMsStr = response.substring(startPos, endPos);
      tempoIrrigacao = timeFertiMsStr.toInt();

      // Mostra os resultados
      Serial.println("start_fertil: " + String(startFertil ? "true" : "false"));
      Serial.println("time_ferti_ms: " + String(tempoIrrigacao));

      // Verifica se a irrigação fértil deve ser iniciada quando start_fertil é verdadeiro
      if (startFertil && tempoIrrigacao > 0) {
        // Ligar o PINO_RELE um tempo antes da irrigação
        digitalWrite(PINO_RELE, HIGH);
        enviarEstadoRele(true); // Envia true para o estado do relé antes da irrigação
        delay(tempoAntesPos); // Atraso antes de iniciar a irrigação

        // Inicia a irrigação
        Serial.println("Iniciando irrigação da fértil...");
        enviarHistoricoFertil();
        digitalWrite(PINO_RELE_FERTIL, HIGH);

        delay(tempoIrrigacao); // Espera durante a irrigação
        Serial.println("Tempo de irrigação: " + String(tempoIrrigacao) + " ms");

        // Desliga a irrigação
        digitalWrite(PINO_RELE_FERTIL, LOW);

        // Desliga o PINO_RELE
        delay(tempoAntesPos); // Atraso após a irrigação
        digitalWrite(PINO_RELE, LOW);
        enviarEstadoRele(false); // Envia false para o estado do relé após a irrigação
        Serial.println("Irrigação da fértil finalizada.");
      } else {
        Serial.println("Irrigação da fértil não será iniciada.");
      }
    } else {
      Serial.println("Erro na requisição: " + String(httpResponseCode));
    }

    // Libera o recurso HTTP
    http.end();
  } else {
    Serial.println("WiFi não está conectado.");
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
