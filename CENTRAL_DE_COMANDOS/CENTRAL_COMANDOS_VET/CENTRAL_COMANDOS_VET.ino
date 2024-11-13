#include <WiFi.h>
#include <FirebaseESP32.h>
#include <DHT.h>
#include <RF24.h>
#include <time.h>
#include <RTClib.h>

// Configurações do WiFi e Firebase
#define WIFI_SSID "ATUALIZE_INTELBRAS"
#define WIFI_PASSWORD "catsix623"

const char* ssid;
const char* password = "v6&48A12";  // Defina a senha da rede escolhida

#define FIREBASE_HOST "https://ainsof-arian-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "AIzaSyAinDXxolmbXRbQ9a_lRyKwLMNrtFxk4R8"

// Configuração de login com e-mail e senha
#define USER_EMAI = "aryan@gmail.com";
#define USER_PASSWORD = "123456";

FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

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

// Estrutura de dados GRUPO
struct GRUPO {
    int QUANTIDADE_SETORES_BOMBA = 0;
    int SETOR_VEZ_BOMBA = 0;
    int BOMBA = 0;
    bool TROCA_DE_SETOR = false;
    bool INICIO_SETUP_IRRIGACAO_BOMBA = false;
    bool BOMBA_ATIVADA = false;

    int SETORES[6];
    int SETOR_ATIVO[6] = {1, 1, 1, 1, 1, 1};
    int SETOR_ANTERIOR = false;

    DateTime INICIO_IRRIGACAO;
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

    bool IRRIGACAO_PARCIAL_TEMPO = false;
    bool IRRIGACAO_INICIADA = false;
    bool CONTINUACAO_IRRIGACAO = false;
    bool IRRIGACAO_ATIVA = false;
    bool HORA_PENDENTE = false;
    int HORA_RESTANTE = 0;
};

GRUPO BOMBAS;




void FIREBASE_CONFIG(){
  // Configuração do Firebase
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
  Firebase.beginStream(firebaseData, "/");

  // Configuração do Firebase
    firebaseConfig.host = FIREBASE_HOST;
    firebaseConfig.api_key = FIREBASE_AUTH;

  
    // Verificar se o login foi bem-sucedido
    if (Firebase.ready()) {
        Serial.println("Login realizado com sucesso!");
    } else {
        Serial.print("Erro ao autenticar: ");
        
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
  
  String formattedTime = String(timeinfo.tm_mday) + "/" + String(timeinfo.tm_mon + 1) + "/" + String(timeinfo.tm_year + 1900) + "-";
  formattedTime += String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec);
  return formattedTime;
}

void setup() {

  Serial.begin(115200);
  dht.begin();   

  // Configuração dos pinos
  pinMode(DHTPIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  CONFIG_RADIO();
  rede();

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
void loop(){

  if (radio.available()) {
        // Tenta receber os dados
        radio.read(&BOMBAS, sizeof(BOMBAS));
            Serial.println("Dados recebidos com sucesso:");
            for (int i = 0; i < 2; i++) {
                Serial.print("Bomba ");
                Serial.println(i);
            }
    }

 
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
  
  enviarGrupoFirebase(BOMBAS, "13/11/24-0:15:0/GRUPO0");
  delay(20000); // Intervalo de 20 segundos entre leituras


}

void enviarGrupoFirebase(GRUPO &grupo, String caminho) {
  
  String path = getFormattedTime();


  // Envia os dados da estrutura para o Firebase no caminho com a data/hora
  Firebase.setInt("grupo/" + path + "/QUANTIDADE_SETORES_BOMBA", grupo.QUANTIDADE_SETORES_BOMBA);
  Firebase.setInt("grupo/" + path + "/SETOR_VEZ_BOMBA", grupo.SETOR_VEZ_BOMBA);
  Firebase.setInt("grupo/" + path + "/BOMBA", grupo.BOMBA);
  Firebase.setBool("grupo/" + path + "/TROCA_DE_SETOR", grupo.TROCA_DE_SETOR);
  Firebase.setBool("grupo/" + path + "/INICIO_SETUP_IRRIGACAO_BOMBA", grupo.INICIO_SETUP_IRRIGACAO_BOMBA);
  Firebase.setBool("grupo/" + path + "/BOMBA_ATIVADA", grupo.BOMBA_ATIVADA);
  
  // Envia arrays de setores
  for (int i = 0; i < 6; i++) {
    Firebase.setInt("grupo/" + path + "/SETORES/" + String(i), grupo.SETORES[i]);
    Firebase.setInt("grupo/" + path + "/SETOR_ATIVO/" + String(i), grupo.SETOR_ATIVO[i]);
    Firebase.setInt("grupo/" + path + "/MINUTOS_SETOR/" + String(i), grupo.MINUTOS_SETOR[i]);
  }

  // Envia os horários e estados adicionais
  Firebase.setInt("grupo/" + path + "/HORA_INICIO", grupo.HORA_INICIO);
  Firebase.setInt("grupo/" + path + "/MINUTO_INICIO", grupo.MINUTO_INICIO);
  Firebase.setInt("grupo/" + path + "/HORA_FIM", grupo.HORA_FIM);
  Firebase.setInt("grupo/" + path + "/MINUTO_FIM", grupo.MINUTO_FIM);

  Serial.println("Dados enviados para o Firebase.");

  Serial.println("Estrutura GRUPO enviada com sucesso!");
}