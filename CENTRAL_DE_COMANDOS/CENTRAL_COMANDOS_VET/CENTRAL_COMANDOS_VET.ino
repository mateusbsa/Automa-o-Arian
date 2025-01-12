#include <WiFi.h>
#include <DHT.h>
#include <RF24.h>
#include <time.h>
#include <RTClib.h>
#include <Firebase_ESP_Client.h>
#include "FS.h"     // Sistema de Arquivos
#include "SPIFFS.h" // SPIFFS no ESP32


// Configurações do WiFi e Firebase
#define WIFI_SSID "ATUALIZE_INTELBRAS"
#define WIFI_PASSWORD "catsix623"

const char* ssid;
const char* password = "v6&48A12";  // Defina a senha da rede escolhida

#define FIREBASE_HOST "https://ainsof-arian-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "AIzaSyAinDXxolmbXRbQ9a_lRyKwLMNrtFxk4R8"

// Configuração de login com e-mail e senha
#define USER_EMAI = "aryan@gmail.com"
#define USER_PASSWORD = "12345"


FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Fuso horário (pode ser ajustado conforme sua localização)
const long gmtOffset_sec = -10800;  // Fuso horário UTC -3 (Brasil)
const int daylightOffset_sec = 3600; // Horário de verão (se aplicável)

RF24 radio(4, 5); // Pinos CE, CSN
const uint64_t address = 0xF0F0F0F0F1LL;
int remoteNodeData[2] = {1, 1,};

// Configurações do DHT11
#define DHTPIN 32      // Porta digital onde o DHT11 está conectado
#define DHTTYPE DHT11  // Define o tipo de DHT
DHT dht(DHTPIN, DHTTYPE);

#define LDR_PIN 35    // Porta analógica onde o LDR está conectado

int INDICE = 0;
FirebaseJson json;
bool firebaseConnected = false;

// Estrutura de dados GRUPO
struct GRUPO {

    int QUANTIDADE_SETORES_BOMBA = 0;
    int SETOR_VEZ_BOMBA = 0;
    int BOMBA = 0;
    bool TROCA_DE_SETOR = false;
    bool INICIO_SETUP_IRRIGACAO_BOMBA = false;
    bool BOMBA_ATIVADA = false;
    
    int SETORES[6];
    int SETOR_ATIVO[6] = {1,1,1,1,1,1};
    int SETOR_ANTERIOR = false;
    
    DateTime INICIO_IRRIGACAO;
    DateTime HORA_INICIO_IRRIGACAO;
    DateTime FIM_IRRIGACAO;
    DateTime INICIO_SETORES[6];
    int HORA_INICIO = 0;
    int MINUTO_INICIO = 0;
    int HORA_FIM = 0;
    int MINUTO_FIM = 0;
    int HORA_FIM_ANTERIOR = 0;
    int MINUTO_FIM_ANTERIOR = 0;

    int MINUTOS_SETOR[6];
    int INTERVALO_SETORES = 1;
    int HORA = 0;
    int MINUTO = 0;
    
    //CONTROLE IRRIGAÇÃO FORA DO TEMPO
    bool IRRIGACAO_PARCIAL_TEMPO = false; //ATIVA FUNÇÃO DE GUARDAR SETOR FORA DO TEMPO DE FUNCIONAMENTO
    bool IRRIGACAO_INICIADA = false; //INFORMA SE JÁ INICIOU A IRRIGAÇÃO
    bool CONTINUACAO_IRRIGACAO = false;  //INFORMA SE HÁ IRRIGAÇÃO A SER CONTINUADA NO PROXIMO HORARIO DE FUNCIONAMENTO
    bool IRRIGACAO_ATIVA = false;
    bool HORA_PENDENTE = false;
    int HORA_RESTANTE = 0;
};
GRUPO BOMBAS[2];


void FIREBASE_CONFIG(){
  // Configuração do Firebase
    config.api_key = FIREBASE_AUTH;
    config.database_url = FIREBASE_HOST;

    // Aqui você pode configurar o token de autenticação se estiver usando autenticação baseada em usuário
    auth.user.email = "aryan@gmail.com";
    auth.user.password = "123456";

    // Inicializa o Firebase
    config.token_status_callback = tokenStatusCallback;  // Callback para status do token
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

  
 // Iniciar o stream com callbacks definidos como lambdas
  if (!Firebase.RTDB.beginStream(&firebaseData, "/status/conectado")) {
    Serial.println("Erro ao iniciar o stream: " + firebaseData.errorReason());
  } else {
    Firebase.RTDB.setStreamCallback(
      &firebaseData,
      [](FirebaseStream data) {  // Callback de stream como lambda
        Serial.println("Stream callback acionado.");
        if (data.dataType() == "int" && data.intData() == 1) {
          enviarParaFirebase(BOMBAS[0]);
        }
      },
      [](bool timeout) {  // Callback de timeout como lambda
        if (timeout) {
          
        }
      }
    );
    firebaseConnected = true;
  }
}

// Função de callback para atualizar o status do token
void tokenStatusCallback(TokenInfo info) {
  if (info.status == token_status_ready) {
    Serial.println("Token pronto e válido.");
  } else if (info.status == token_status_error) {
    Serial.printf("Erro no token: %s\n", info.error.message.c_str());
  }
}

void CONFIG_RADIO(){
    radio.begin();
    //radio.setPALevel(RF24_PA_MAX); // Nível de potência do transmissor
    //radio.setDataRate(RF24_1MBPS);
    radio.openReadingPipe(0, address); // Muda para o modo de recebimento
    // enable ack payload - slave replies with data using this feature
    radio.enableAckPayload();
    // preload payload with initial data
    radio.writeAckPayload(0, &remoteNodeData, sizeof(remoteNodeData));
    radio.startListening();
}

// Função para formatar a hora no formato d/m/a-h:m:s
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter a hora");
    return "";
  }
  // Usando sprintf para formatar a hora, minuto e segundo com dois dígitos
  char buffer[20];
  sprintf(buffer, "%02d|%02d|%04d-%02d:%02d:%02d", 
          timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, 
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  
  return String(buffer);
}

void setup() {

  Serial.begin(115200);
  dht.begin();   

  // Configuração dos pinos
  pinMode(DHTPIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  CONFIG_RADIO();
  //rede();
  //FIREBASE_CONFIG();
   // Configura o NTP
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");

}

void rede(){
  Serial.println("Escaneando redes WiFi...");
  int numeroRedes = WiFi.scanNetworks();  // Escaneia redes disponíveis
  
  if (numeroRedes == 0) {
    Serial.println("Nenhuma rede encontrada.");
  } else {
    Serial.println("Redes encontradas:");
    for (int i = 0; i < numeroRedes; ++i) {
      // Exibe cada rede com índice, nome (SSID) e intensidade do sinal (RSSI)
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (Sinal: ");
      Serial.print(WiFi.RSSI(i));
      Serial.println(" dBm)");
    }

    // Escolher rede manualmente no código
    int indiceRede = 0;  // Alterar para o índice desejado
    ssid = WiFi.SSID(indiceRede).c_str();

    Serial.print("Conectando a ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED) {  // Limita a 20 tentativas
      delay(500);
      Serial.print(".");
      tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConectado com sucesso!");
      Serial.print("IP obtido: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFalha ao conectar.");
    }
  }
}


void SET_LER_RADIO(){
    radio.openReadingPipe(0, address); // Muda para o modo de recebimento
    radio.startListening();
    delay(250);
}

void SET_ENVIA_RADIO(){
    radio.stopListening();  // Muda para o modo de transmissão
    radio.openWritingPipe(address);
    delay(250);
}
bool cont = 0;


void exibirBufferSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Falha ao montar SPIFFS");
        return;
    }
    
    File file = SPIFFS.open("/buffer.json", FILE_READ);
    if (!file) {
        Serial.println("Erro ao abrir o arquivo de buffer para leitura");
        return;
    }

    Serial.println("Exibindo dados do buffer:");
    while (file.available()) {
        String linha = file.readStringUntil('\n'); // Lê cada linha do arquivo
        Serial.println(linha); // Exibe a linha na Serial
    }
    
    file.close();
}


void loop(){

  /*if (!firebaseConnected) {
    Serial.println("Reconectando ao Firebase...");
    FIREBASE_CONFIG();
  }*/

  if (Serial.available() > 0) {
        char comando = Serial.read();
        
        if (comando == '0') {
            Serial.println("Comando '0' recebido: Exibindo buffer...");
            exibirBufferSPIFFS();
        }
    }

  if (radio.available()) {
        // Tenta receber os dados
        radio.read(&BOMBAS[0], sizeof(BOMBAS[0]));
            Serial.println("Dados recebidos com sucesso:");
            gerarLog();
           /* for (int i = 0; i < 2; i++) {
                Serial.print("Bomba ");
                Serial.println(i);
            }*/
            // Leitura dos dados do DHT11
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  // Verifica se as leituras são válidas
  
    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" °C");

    Serial.print("Umidade: ");
    Serial.print(umidade);
    Serial.println(" %");
  

  // Leitura do LDR
  int valorLDR = analogRead(LDR_PIN);
  Serial.print("Valor do LDR: ");
  Serial.println(valorLDR);
    }

 
  
  
  //enviarParaFirebase(BOMBAS);
  //delay(6000); // Intervalo de 20 segundos entre leituras


}

// Função para converter a estrutura em JSON
void estruturaParaJson(GRUPO grupo, FirebaseJson &json) {
    json.set("QUANTIDADE_SETORES_BOMBA", grupo.QUANTIDADE_SETORES_BOMBA);
    json.set("SETOR_VEZ_BOMBA", grupo.SETOR_VEZ_BOMBA);
    json.set("BOMBA", grupo.BOMBA);
    json.set("TROCA_DE_SETOR", grupo.TROCA_DE_SETOR);
    json.set("INICIO_SETUP_IRRIGACAO_BOMBA", grupo.INICIO_SETUP_IRRIGACAO_BOMBA);
    json.set("BOMBA_ATIVADA", grupo.BOMBA_ATIVADA);

    FirebaseJsonArray setoresArray;
    for (int i = 0; i < 6; i++) {
        setoresArray.add(grupo.SETORES[i]);
    }
    json.set("SETORES", setoresArray);

    FirebaseJsonArray setorAtivoArray;
    for (int i = 0; i < 6; i++) {
        setorAtivoArray.add(grupo.SETOR_ATIVO[i]);
    }
    json.set("SETOR_ATIVO", setorAtivoArray);

    json.set("SETOR_ANTERIOR", grupo.SETOR_ANTERIOR);
    json.set("HORA_INICIO", grupo.HORA_INICIO);
    json.set("MINUTO_INICIO", grupo.MINUTO_INICIO);
    json.set("HORA_FIM", grupo.HORA_FIM);
    json.set("MINUTO_FIM", grupo.MINUTO_FIM);
    json.set("HORA_FIM_ANTERIOR", grupo.HORA_FIM_ANTERIOR);
    json.set("MINUTO_FIM_ANTERIOR", grupo.MINUTO_FIM_ANTERIOR);

    FirebaseJsonArray minutosSetorArray;
    for (int i = 0; i < 6; i++) {
        minutosSetorArray.add(grupo.MINUTOS_SETOR[i]);
    }
    json.set("MINUTOS_SETOR", minutosSetorArray);

    json.set("INTERVALO_SETORES", grupo.INTERVALO_SETORES);
    json.set("HORA", grupo.HORA);
    json.set("MINUTO", grupo.MINUTO);

    json.set("IRRIGACAO_PARCIAL_TEMPO", grupo.IRRIGACAO_PARCIAL_TEMPO);
    json.set("IRRIGACAO_INICIADA", grupo.IRRIGACAO_INICIADA);
    json.set("CONTINUACAO_IRRIGACAO", grupo.CONTINUACAO_IRRIGACAO);
    json.set("IRRIGACAO_ATIVA", grupo.IRRIGACAO_ATIVA);
    json.set("HORA_PENDENTE", grupo.HORA_PENDENTE);
    json.set("HORA_RESTANTE", grupo.HORA_RESTANTE);
}

// Função principal para envio ao Firebase e escuta do rádio
void enviarParaFirebase(GRUPO grupo) {
    FirebaseJson json;
    estruturaParaJson(grupo, json);
    String formattedTime = getFormattedTime();

    if (Firebase.RTDB.setJSON(&firebaseData, "/grupo0/" + formattedTime, &json)) {
        Serial.println("Dados enviados com sucesso!");
    } else {
        Serial.println("Falha ao enviar os dados: " + firebaseData.errorReason());
        salvarBufferSPIFFS(json); // Salva o JSON no buffer SPIFF
    }
    if(checkBufferSize() > 10){
      sendDataFromBuffer();
    }
}

void salvarBufferSPIFFS(FirebaseJson &json) {
  if (!SPIFFS.begin(true)) {
        Serial.println("Falha ao montar SPIFFS");
        return;
    }
     File file = SPIFFS.open("/buffer.txt", FILE_APPEND);
  if (!file) {
    Serial.println("Erro ao abrir o arquivo.");
    return;
  }

  String timeReceived = getFormattedTime();
  String dataToSave = timeReceived +"\n";  // Armazena os dados e hora de recebimento

  file.print(dataToSave);
  file.close();
  Serial.println("Dados salvos no buffer.");
}

void sendDataFromBuffer() {
  File file = SPIFFS.open("/buffer.txt", FILE_READ);
  if (!file) {
    Serial.println("Erro ao abrir o arquivo de buffer.");
    return;
  }

  String line;
  while (file.available()) {
    line = file.readStringUntil('\n');
    Serial.println("Enviando: " + line);
    // Enviar os dados para o Firebase
    FirebaseJson json;
    json.set("Data", line);
    String formattedTime = getFormattedTime();
    if (Firebase.RTDB.setJSON(&firebaseData, "/grupo0/" + formattedTime, &json)) {
      Serial.println("Dados enviados com sucesso!");
    } else {
      Serial.println("Falha ao enviar os dados: " + firebaseData.errorReason());
    }
  }
  file.close();

  if(WiFi.status() == WL_CONNECTED){
    clearBuffer();
  }
  
}

size_t checkBufferSize() {
  File file = SPIFFS.open("/buffer.txt", FILE_READ);
  if (!file) {
    Serial.println("Erro ao abrir o arquivo.");
    return 0;
  }

  // Obtém o tamanho do arquivo
  size_t fileSize = file.size();

  // Exibe o tamanho do arquivo (buffer) em bytes
  Serial.print("Tamanho do buffer: ");
  Serial.print(fileSize);
  Serial.println(" bytes");

  file.close();

  return fileSize;
}

void clearBuffer() {
  File file = SPIFFS.open("/buffer.txt", FILE_WRITE); // Abre o arquivo em modo de escrita
  if (!file) {
    Serial.println("Erro ao abrir o arquivo para limpar.");
    return;
  }

  file.close();  // Fecha o arquivo (depois de abrir, ele é truncado automaticamente em modo FILE_WRITE)
  
  Serial.println("Buffer limpo com sucesso!");
}

void gerarLog() {
    Serial.println("===== LOG DO SISTEMA =====");

    // Exibe o estado de cada variável do grupo de bombas
    for (int i = 0; i < 1; i++) {
        Serial.print("Grupo: ");
        Serial.println(i);

        GRUPO& grupo = BOMBAS[i];

        // Exiba as variáveis do grupo no Serial Monitor
        Serial.print("Quantidade de Setores: ");
        Serial.println(grupo.QUANTIDADE_SETORES_BOMBA);
        Serial.print("Setor da vez: ");
        Serial.println(grupo.SETOR_VEZ_BOMBA);
        Serial.print("Bomba ativada: ");
        Serial.println(grupo.BOMBA_ATIVADA);
        
        Serial.print("Setores: ");
        for (int i = 0; i < grupo.QUANTIDADE_SETORES_BOMBA; i++) {
            Serial.print("Setor ");
            Serial.print(i);
            Serial.print(": ");
            Serial.println(grupo.SETORES[i]);
        }
        
        Serial.print("Setor Ativo: ");
        for (int i = 0; i < grupo.QUANTIDADE_SETORES_BOMBA; i++) {
            if(i > 0){
                Serial.print("Setor Ativo ");
                Serial.print(i);
                Serial.print(": ");
                Serial.println(grupo.SETOR_ATIVO[i]);
            }
            
        }
        
        Serial.print("INICIO_IRRIGACAO: ");
        Serial.println(grupo.HORA_INICIO_IRRIGACAO.timestamp()); // Supondo que seja um timestamp
        for (int i = 0; i < grupo.QUANTIDADE_SETORES_BOMBA; i++) {
            Serial.print("Setor ");
            Serial.print(i);
            Serial.print(": ");
            Serial.print(grupo.SETOR_ATIVO[i]);
            Serial.print(" : ");
            Serial.println(grupo.INICIO_SETORES[i].timestamp());
        }
        Serial.print("FIM_IRRIGACAO: ");
        Serial.println(grupo.FIM_IRRIGACAO.timestamp()); // Supondo que seja um timestamp
        Serial.print("HORA_INICIO: ");
        Serial.println(grupo.HORA_INICIO);
        Serial.print("MINUTO_INICIO: ");
        Serial.println(grupo.MINUTO_INICIO);
        Serial.print("HORA_FIM: ");
        Serial.println(grupo.HORA_FIM);
        Serial.print("MINUTO_FIM: ");
        Serial.println(grupo.MINUTO_FIM);
        
        Serial.print("IRRIGACAO_PARCIAL_TEMPO: ");
        Serial.println(grupo.IRRIGACAO_PARCIAL_TEMPO);
        Serial.print("IRRIGACAO_INICIADA: ");
        Serial.println(grupo.IRRIGACAO_INICIADA);
        Serial.print("CONTINUACAO_IRRIGACAO: ");
        Serial.println(grupo.CONTINUACAO_IRRIGACAO);
        
        Serial.println("============================");
    }

 
}