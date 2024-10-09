# Sistema de Irrigação Automatizado com ESP32

Este projeto faz parte de um sistema de irrigação automatizado que utiliza uma ESP32 para coletar dados de umidade do solo e do ar, controlar a irrigação com um relé, e comunicar esses dados com um sistema Django hospedado remotamente.

## Componentes Utilizados

- **ESP32**: Microcontrolador principal.
- **DHT22**: Sensor de temperatura e umidade do ar.
- **Sensores de Umidade do Solo**: Sensores analógicos para medir a umidade do solo.
- **Relé**: Para controlar o sistema de irrigação.
- **LED ou Luz de Erro**: Para indicar problemas na conexão.

## Funcionalidades

1. **Leitura de Sensores**: O sistema lê a umidade do solo de dois sensores analógicos e a temperatura e umidade do ar de um sensor DHT22.
2. **Envio de Dados para o Servidor**: As leituras dos sensores são enviadas para um servidor Django, que processa os dados e monitora o sistema.
3. **Controle de Irrigação**: O sistema ativa a irrigação automaticamente com base nos níveis de umidade do solo. Se o solo estiver muito seco, o relé é acionado para ligar a irrigação.
4. **Monitoramento Remoto**: O estado do sistema de irrigação (se está ligado ou desligado) é enviado ao servidor para monitoramento.
5. **Tratamento de Erros**: Caso o ESP32 perca a conexão com a rede WiFi, uma luz de erro é acesa para indicar o problema.

## Diagrama de Conexão

- **Sensor de Umidade 1**: Conectado ao pino analógico 37.
- **Sensor de Umidade 2**: Conectado ao pino analógico 38.
- **Sensor DHT22**: Conectado ao pino digital 32.
- **Relé**: Conectado ao pino digital 25 para controlar o sistema de irrigação.
- **Luz de Erro**: Conectada ao pino digital 21.

## Configuração

### Rede WiFi

- SSID: `----`
- Senha: `----`

### URLs do Servidor

- URL de coleta de dados: `https://----/collect-data/`
- URL de estado do relé: `https://----/collect-solenoid-state/`
- Chave de autenticação: 

### Calibração dos Sensores

Os sensores de umidade do solo foram calibrados com os seguintes valores:

- **Valor mínimo**: 4095 (solo seco).
- **Valor máximo**: 854 (solo muito úmido).

### Intervalos de Leitura e Controle

- **Leitura dos Sensores**: A cada 15 minutos (900.000 ms).
- **Verificação da Irrigação**: A cada 1 segundo.

## Fluxo de Funcionamento

1. **Conexão com o WiFi**: A ESP32 tenta se conectar à rede WiFi especificada. Se a conexão falhar, a luz de erro é acesa.
2. **Leitura dos Sensores**: A cada 15 minutos, os dados dos sensores de umidade do solo e do ar são lidos.
3. **Controle da Irrigação**: A cada segundo, o sistema verifica os níveis de umidade e decide se deve ligar ou desligar o relé.
4. **Envio de Dados**: Os dados lidos são enviados para o servidor remoto via HTTP.
5. **Envio do Estado do Relé**: Sempre que o estado do relé muda (ligado/desligado), essa informação é enviada para o servidor.

## Erros e Soluções

- **Erro de Conexão WiFi**: Se o dispositivo perder a conexão com a rede, a luz de erro será acesa até que a conexão seja restabelecida.
- **Falha na Leitura do DHT**: Se houver falha na leitura dos sensores de temperatura ou umidade do ar, a leitura é ignorada e uma mensagem de erro é registrada no console.

## Como Utilizar

1. Faça o upload do código para a ESP32.
2. Conecte os sensores e o relé conforme o diagrama de conexão.
3. Verifique se a ESP32 está conectada ao WiFi e se os dados estão sendo enviados corretamente para o servidor.
4. Monitore e controle o sistema de irrigação remotamente através da interface web no servidor Django.

## Considerações Finais

Este sistema é ideal para pequenos projetos de automação de irrigação, onde o monitoramento remoto e o controle automático são essenciais. O uso de um microcontrolador como a ESP32 com integração em um sistema web permite uma solução flexível e escalável.
