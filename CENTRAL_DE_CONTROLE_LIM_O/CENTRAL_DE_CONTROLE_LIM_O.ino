#include <RF24.h>
#include "RTClib.h"
#include <EEPROM.h>   
#include "FS.h"     // Sistema de Arquivos
#include "SPIFFS.h" // SPIFFS no ESP32
#include <ArduinoJson.h>

hw_timer_t *timer = NULL; //faz o controle do temporizador (interrupção por tempo)

// VERSÃO DO SOFTWARE PLACA ESP32 ARDUINO IDE 1.0.5

/*
OUTPUT NÃO FUNCIONA -> 34 | 35 / 36 | 39

PINOS DAC -> 25 | 26

PINOS QUE OSCILAM -> 1 | 3 | 5 | 6 | 7 | 8 | 11 | 14 | 15
*/

/*
___ PORTAS USADAS ___

NRF24L01 -> 4 | 5 | 18 | 19 | 23

RTC -> 21 | 22

SOLENOIDES -> 12 | 13 | 14 | 2 | 27 | 33

BOMBA -> 15

SENSOR DE CORRENTE -> 25

SENSOR DE TENSÃO -> 26

SENSOR DE VAZÃO ->  32

*/

//CONFIGURAÇÃO EEPROM

#define EEPROM_SIZE 64     // Tamanho da EEPROM simulada
#define NUM_SETORES 6      // Número de setores de irrigação


// DEFINIÇÃO DAS PORTAS

const int BOMBA1 = 15; //ACIONAMENTO DA ELETROBOMBA
const int BOMBA2 = 32; //ACIONAMENTO DA ELETROBOMBA

const int SENSOR_CORRENTE = 1; //INPUT SENSOR DE CORRENTE AC
const int SENSOR_TENSAO = 3; //INPUT SENSOR DE TENSÃO AC
//const int SENSOR_VAZAO = 32; //INPUT SENSOR DE VAZÃO

const int SOLENOIDE_A = 25; //ACIONAMENTO PORTA DE CONTROLE SETOR
const int SOLENOIDE_B = 13; //ACIONAMENTO PORTA DE CONTROLE SETOR
const int SOLENOIDE_C = 14; //ACIONAMENTO PORTA DE CONTROLE SETOR
const int SOLENOIDE_D = 2; //ACIONAMENTO PORTA DE CONTROLE SETOR
const int SOLENOIDE_E = 27; //ACIONAMENTO PORTA DE CONTROLE SETOR
const int SOLENOIDE_F = 33; //ACIONAMENTO PORTA DE CONTROLE SETOR 

const int CE_NRF = 4;
const int CSN_NRF = 5;


/* VETOR DE CONFIGURAÇÃO 
 
1 -> QUANTIDADE DE VALVULAS SOLENOIDES
2 -> 
*/

/* VETOR DE CONTROLE COMANDO
0 -> SETOR A
1 -> SETOR B
2 -> SETOR C
3 -> SETOR D
4 -> SETOR E
5 -> SETOR F
6 -> BOMBA
7 -> INICIANDO IRRIGAÇÃO
8 -> ENCERRANDO IRRIGAÇÃO
9 ->
 
*/

// DEFINIÇÕES DO FIREBASE

int setores_bomba1_firebase[6] = {};
int setores_bomba2_firebase[6] = {};

int IRRIGADOS[2] = {0,0};   

int indica_inicio = 1001;  //ENVIA PARA CENTRAL DE COMANDOS O SINAL DEQUE ESTÁ PRONTO PARA RECEBER O VETOR DE CONFIGURAÇÃO

// DEFINIÇÕES DO DEFAULT

int SETORES_BOMBA1[6] = {SOLENOIDE_A,SOLENOIDE_B,-1,-1,-1,-1};
int SETORES_BOMBA2[6] = {SOLENOIDE_C,SOLENOIDE_D,SOLENOIDE_E,SOLENOIDE_F,-1,-1};
//int MINUTOS_SETORES[6] = {240,240,120,120,120,120};
//int MINUTOS_SETORES[6] = {5,5,5,5,5,5};
int MINUTOS_SETORES[6] = {2,2,2,2,2,2};
//int HORA_FUNCIONAMENTO[2] = {5,17};
int HORA_FUNCIONAMENTO[2] = {5,16};


//int MINUTOS_SETORES[6] = {2,2,5,5,5,5};

// VARIÁVEIS GLOBAIS

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



//SETORES VARIÁVEIS GLOBAIS
char SETORES[6] = {SOLENOIDE_A,SOLENOIDE_B,SOLENOIDE_C,SOLENOIDE_D,SOLENOIDE_E,SOLENOIDE_F};


//BOMBAS VARIÁVEIS GLOBAIS
int BUFFER_ENVIO[100][6];
int POSICAO_BUFFER = 0;

//RÁDIO NRF24L01
const uint64_t address = 0xF0F0F0F0F1LL;
RF24 radio(CE_NRF, CSN_NRF); // Pinos CE, CSN
int data_ant_recive = 0;

//RTC HORA
RTC_DS1307 rtc;
DateTime INICIO_IRRIGACAO;

char daysOfTheWeek[7][12] = {"DOMINGO", "SEGUNDA", "TERCA", "QUARTA", "QUINTA", "SEXTA", "SABADO"};


int dados_buffer = 0;
int cont = 0;
int minuto = 0;
int segundo = 0;
int minuto_atual = 0;

int DATA_ENVIO[6];

//REGISTRO OPERAÇÃO SETORES
int REGISTRO[20];
char REGISTRO_DATA;
static const char *palavras[] = {
    "inicia", "encerra", "bomba1", "bomba2", "setor:a1", "setor:b1", "setor:c1", "setor:d1", "setor:e1", 
    "setor:f1","setor:a0", "setor:b0", "setor:c0", "setor:d0", "setor:e0", "setor:f0"
  };

// Inicializando SPIFFS e RTC
void iniciarSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Erro ao montar SPIFFS.");
        return;
    }
    Serial.println("SPIFFS montado com sucesso.");
}

void config(){

    //Adicionando setores
    registrarLog("Iniciando configuração das bombas.");
    int cont = 0;

    for(int i=0; i<6; i++){
        if(SETORES_BOMBA1[i] != -1){
        BOMBAS[0].SETORES[cont] = SETORES_BOMBA1[i];
        cont++;
        }
    }
    
    cont = 0;

    for(int i=0; i<6; i++){
        if(SETORES_BOMBA2[i] != -1){
        BOMBAS[1].SETORES[cont] = SETORES_BOMBA2[i];
        cont++;
        }
    }

    for(int i = 0; i < 6; i++){
        if(BOMBAS[0].SETORES[i] > 0){
            BOMBAS[0].QUANTIDADE_SETORES_BOMBA +=1;
        }
    }

    for(int i = 0; i < 6; i++){
        if(BOMBAS[1].SETORES[i] > 0){
            BOMBAS[1].QUANTIDADE_SETORES_BOMBA +=1;
        }
    }

    for(int i = 0; i<2;i++){
        for(int j = 0; j<6; j++){
            if(BOMBAS[i].SETORES[j]== SOLENOIDE_A){
                BOMBAS[i].MINUTOS_SETOR[BOMBAS[i].SETOR_VEZ_BOMBA]=MINUTOS_SETORES[0];
                BOMBAS[i].SETOR_VEZ_BOMBA += 1;
            }else if(BOMBAS[i].SETORES[j]== SOLENOIDE_B){
                BOMBAS[i].MINUTOS_SETOR[BOMBAS[i].SETOR_VEZ_BOMBA]=MINUTOS_SETORES[1];
                BOMBAS[i].SETOR_VEZ_BOMBA += 1;
            }else if(BOMBAS[i].SETORES[j]== SOLENOIDE_C){
                BOMBAS[i].MINUTOS_SETOR[BOMBAS[i].SETOR_VEZ_BOMBA]=MINUTOS_SETORES[2];
                BOMBAS[i].SETOR_VEZ_BOMBA += 1;
            }else if(BOMBAS[i].SETORES[j]== SOLENOIDE_D){
                BOMBAS[i].MINUTOS_SETOR[BOMBAS[i].SETOR_VEZ_BOMBA]=MINUTOS_SETORES[3];
                BOMBAS[i].SETOR_VEZ_BOMBA += 1;
            }else if(BOMBAS[i].SETORES[j]== SOLENOIDE_E){
                BOMBAS[i].MINUTOS_SETOR[BOMBAS[i].SETOR_VEZ_BOMBA]=MINUTOS_SETORES[4];
                BOMBAS[i].SETOR_VEZ_BOMBA += 1;
            }else if(BOMBAS[i].SETORES[j]== SOLENOIDE_F){
                BOMBAS[i].MINUTOS_SETOR[BOMBAS[i].SETOR_VEZ_BOMBA]=MINUTOS_SETORES[5];
                BOMBAS[i].SETOR_VEZ_BOMBA += 1;
            }
        }
    }
    BOMBAS[0].SETOR_VEZ_BOMBA = 0;
    BOMBAS[1].SETOR_VEZ_BOMBA = 0;

    //Adicionando pora Bomba
    BOMBAS[0].BOMBA = BOMBA1;
    BOMBAS[1].BOMBA = BOMBA2;

    //HORA INICIO ATUAL TEMPORARIO
    DateTime now = rtc.now();
    BOMBAS[0].HORA_INICIO = now.hour();
    BOMBAS[0].MINUTO_INICIO = now.minute();

    BOMBAS[1].HORA_INICIO = now.hour();
    BOMBAS[1].MINUTO_INICIO = now.minute();

    int quantidade_setores = BOMBAS[0].QUANTIDADE_SETORES_BOMBA + BOMBAS[1].QUANTIDADE_SETORES_BOMBA;

    registrarLog("Configuração concluída com sucesso.");

}


/*
int SINAL_LED(string SINAL){

    switch(SINAL) {
        case "inicializando":
            // code block
            break;
        case "enviando dados":
            // code block
            break;
        case "falha rtc":
            // code block
            break;
        default:
            // code block
    }
}
*/
void PINOUT_SETUP() {

registrarLog("Iniciando configuração dos pinos.");

    // Configuração dos pinos como OUTPUT com delay
    pinMode(BOMBA1, OUTPUT); delay(50); registrarLog("BOMBA1 configurada como OUTPUT.");
    pinMode(BOMBA2, OUTPUT); delay(50); registrarLog("BOMBA2 configurada como OUTPUT.");
    
    pinMode(SOLENOIDE_A, OUTPUT); delay(50); registrarLog("SOLENOIDE_A configurado como OUTPUT.");
    pinMode(SOLENOIDE_B, OUTPUT); delay(50); registrarLog("SOLENOIDE_B configurado como OUTPUT.");
    pinMode(SOLENOIDE_C, OUTPUT); delay(50); registrarLog("SOLENOIDE_C configurado como OUTPUT.");
    pinMode(SOLENOIDE_D, OUTPUT); delay(50); registrarLog("SOLENOIDE_D configurado como OUTPUT.");
    pinMode(SOLENOIDE_E, OUTPUT); delay(50); registrarLog( "SOLENOIDE_E configurado como OUTPUT.");
    pinMode(SOLENOIDE_F, OUTPUT); delay(50); registrarLog( "SOLENOIDE_F configurado como OUTPUT.");

    // Ativação dos pinos com HIGH e log de status
    digitalWrite(BOMBA1, HIGH); delay(20); registrarLog("BOMBA1 desativada (HIGH).");
    digitalWrite(BOMBA2, HIGH); delay(20); registrarLog("BOMBA2 desativada (HIGH).");

    digitalWrite(SOLENOIDE_A, HIGH); delay(20); registrarLog("SOLENOIDE_A desativada (HIGH).");
    digitalWrite(SOLENOIDE_B, HIGH); delay(20); registrarLog("SOLENOIDE_B desativada (HIGH).");
    digitalWrite(SOLENOIDE_C, HIGH); delay(20); registrarLog("SOLENOIDE_C desativada (HIGH).");
    digitalWrite(SOLENOIDE_D, HIGH); delay(20); registrarLog("SOLENOIDE_D desativada (HIGH).");
    digitalWrite(SOLENOIDE_E, HIGH); delay(20); registrarLog("SOLENOIDE_E desativada (HIGH).");
    digitalWrite(SOLENOIDE_F, HIGH); delay(20); registrarLog("SOLENOIDE_F desativada (HIGH).");

    registrarLog("Configuração dos pinos concluída.");
}

void SETRTC(){
    
    #ifndef ESP8266
  while (!Serial); // wait for serial port to connect. Needed for native USB
    #endif

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    //SINAL_LED("falha rtc");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
   //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    //rtc.adjust(DateTime(2014, 3, 16, 1, 8, 0));
  }

  // When time needs to be re-set on a previously configured device, the
  // following line sets the RTC to the date & time this sketch was compiled
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // This line sets the RTC with an explicit date & time, for example to set
  // January 21, 2014 at 3am you would call:
  //rtc.adjust(DateTime(2024, 4, 23, 14, 24, 0));
  DateTime now = rtc.now();
  segundo = now.minute();
  minuto = now.minute();
}

void SET_RADIO(){

    registrarLog("Iniciando configuração do rádio.");

    // Iniciando o rádio
    radio.begin();
    // Configurando o nível de potência do transmissor
    radio.setPALevel(RF24_PA_MAX);
    registrarLog("Nível de potência do transmissor configurado para RF24_PA_MAX.");

    // Configurando a taxa de dados
    radio.setDataRate(RF24_1MBPS);
    registrarLog("Taxa de dados configurada para 1MBPS.");

    // Habilitando a carga útil de confirmação
    radio.enableAckPayload();
    registrarLog("Carga útil de confirmação habilitada.");

    // Chamada para função que define as operações de leitura do rádio
    SET_LER_RADIO();
}

void SET_LER_RADIO(){

    registrarLog("Abrindo o pipe de leitura.");
    
    radio.openReadingPipe(0, address);
    registrarLog("Pipe de leitura aberto com sucesso.");

    radio.startListening(); // Inicia a escuta
    delay(250);
    
    registrarLog("Iniciando escuta do rádio.");
}

void SET_ENVIA_RADIO(){
    registrarLog("Mudando para o modo de transmissão.");
    radio.stopListening();  // Muda para o modo de transmissão
    radio.openWritingPipe(address);
    registrarLog("Pipe de escrita aberto com sucesso.");
    delay(250);
    registrarLog("Modo de transmissão pronto.");
}

//função que o temporizador irá chamar, para reiniciar o ESP32
void IRAM_ATTR resetModule(){
    //ets_printf("(watchdog) reiniciar\n"); //imprime no log
    esp_restart(); //reinicia o chip
}

int est = 0;
void setup() {
    //SINAL_LED("inicializando");
    Serial.begin(115200);
    SETRTC();
    iniciarSPIFFS();  // Inicializa o SPIFFS
    config();

    PINOUT_SETUP();
    
    SET_RADIO();

    
    //delay(3000);

    SETORES_ATIVOS(0);  //grupo da bomba 1
    SETORES_ATIVOS(1);  //grupo da bomba 2

    //timerID 0, div 80 (clock do esp), contador progressivo
    timer = timerBegin(0, 80, true); //timerID 0, div 80
    //timer, callback, interrupção de borda
    timerAttachInterrupt(timer, &resetModule, true);
    //timer, tempo (us), repetição
    timerAlarmWrite(timer, 40000000, true);
    timerAlarmEnable(timer); //habilita a interrupção 
    
}



void loop() {

    timerWrite(timer, 0); //reseta o temporizador (alimenta o watchdog) 
    long tme = millis(); //tempo inicial do loop

    DateTime now = rtc.now();

    if (radio.available()) {
        int command;
        radio.read(&command, sizeof(command));
        Serial.println(command);
        // Execute a ação correspondente ao comando recebido
        if (command == 100) {
           CONFIGURE_SETUP_REMOTO();
        }else if(command == 101){
            ENVIA_STATUS_ATUAL();
        }
        
    }
/*
    if(Serial.available() > 0){
        int recebi = Serial.read();
        Serial.println(recebi);
        if(recebi == 48){
            Serial.println("BOMBA 1");
            STATUS_SERIAL(0);
            Serial.println("BOMBA 2");
            STATUS_SERIAL(1);
        
        }else if(recebi == 49){
           SERIAL_TIME(now);
        }
    }
*/    
    if (Serial.available() > 0) {
        char entrada = Serial.read();  // Lê a entrada

        if (entrada == '1') {  // Se o usuário apertar '1'
            gerarLog();  // Exibe o log no serial
        }
        else if (entrada == '2') { // Verifica se o número digitado é 2
            exibirTamanhosArquivos(); // Chama a função para exibir tamanhos dos arquivos
        }
        else if (entrada == '3') { // Verifica se o número digitado é 2
            deletarTodosArquivos(); // Chama a função para exibir tamanhos dos arquivos
        }
        else if(entrada == '4'){
            lerLogs();
        }
        else if(entrada == '5'){
            exibirDados();
        }
        else if(entrada == '6'){
            SPIFFS.remove("/dados.bin");
        }
    }
    
    

/*
    if(now.hour() == BOMBAS[0].HORA_INICIO && now.minute() == BOMBAS[0].MINUTO_INICIO && BOMBAS[0].BOMBA_ATIVADA  == false){
        INCIA_IRRIGACAO(0);
    }
    
    if(now.hour() == BOMBAS[1].HORA_INICIO && now.minute() == BOMBAS[1].MINUTO_INICIO && BOMBAS[1].BOMBA_ATIVADA  == false){
        INCIA_IRRIGACAO(1);
    }
*/
    
    //INICIA PROCESSO DE IRRIGAÇÃO E CONTINUAÇÃO
    if(now.hour() >= HORA_FUNCIONAMENTO[0] && BOMBAS[0].BOMBA_ATIVADA  == false && now.hour()< HORA_FUNCIONAMENTO[1]){
        
        //CONTINUAÇÃO DOS SETORES QUE NÃO TIVEREM TEMPO DENTRO DO HORARIO
         if(BOMBAS[0].IRRIGACAO_INICIADA && BOMBAS[0].CONTINUACAO_IRRIGACAO && now.hour() == BOMBAS[0].HORA_INICIO){
            Serial.println("Continuar irrigação");
            IRRIGACAO(0);
            BOMBAS[0].SETOR_ANTERIOR = true;

         }else if(!BOMBAS[0].IRRIGACAO_INICIADA && now.hour() == BOMBAS[0].HORA_INICIO){
            Serial.println("Sem pendencias");
            INCIA_IRRIGACAO(0);
         }
    }
  
    if(now.hour() >= HORA_FUNCIONAMENTO[0] && BOMBAS[1].BOMBA_ATIVADA  == false && now.hour()< HORA_FUNCIONAMENTO[1]){
        
        //CONTINUAÇÃO DOS SETORES QUE NÃO TIVEREM TEMPO DENTRO DO HORARIO
         if(BOMBAS[1].IRRIGACAO_INICIADA && BOMBAS[1].CONTINUACAO_IRRIGACAO && now.hour() == BOMBAS[0].HORA_INICIO){
            Serial.println("Continuar irrigação");
            IRRIGACAO(1);
            BOMBAS[1].SETOR_ANTERIOR = true;

         }else if(!BOMBAS[1].IRRIGACAO_INICIADA && now.hour() == BOMBAS[1].HORA_INICIO){
            Serial.println("Sem pendencias");
            INCIA_IRRIGACAO(1);
         }
    }

   /*if(now.hour() == BOMBAS[0].HORA_FIM && now.minute() >= BOMBAS[0].MINUTO_FIM && BOMBAS[0].BOMBA_ATIVADA  == true && BOMBAS[0].CONTINUACAO_IRRIGACAO){
        BOMBAS[0].CONTINUACAO_IRRIGACAO = false;
        BOMBAS[0].IRRIGACAO_INICIADA = false;
        FINALIZA_IRRIGACAO(0);
    }*/ 
    
    if(now.hour() == BOMBAS[0].HORA_FIM_ANTERIOR && now.minute() == BOMBAS[0].MINUTO_FIM_ANTERIOR && BOMBAS[0].BOMBA_ATIVADA  == true && BOMBAS[0].SETOR_ANTERIOR == true){
        Serial.println("TE ACEHI");
        ENCERRA_SETOR_ANTERIOR(0);
    }

    if(now.hour() == BOMBAS[1].HORA_FIM_ANTERIOR && now.minute() == BOMBAS[1].MINUTO_FIM_ANTERIOR && BOMBAS[1].BOMBA_ATIVADA  == true && BOMBAS[1].SETOR_ANTERIOR == true){
        ENCERRA_SETOR_ANTERIOR(1);
    }

    if(now.hour() == HORA_FUNCIONAMENTO[1] && BOMBAS[0].BOMBA_ATIVADA == true && BOMBAS[0].IRRIGACAO_PARCIAL_TEMPO){
        Serial.println("Malha finha");
        PROXIMOSETOR(now, 0);
    }

    if(now.hour() == HORA_FUNCIONAMENTO[1] && BOMBAS[1].BOMBA_ATIVADA == true && BOMBAS[1].IRRIGACAO_PARCIAL_TEMPO){
        Serial.println("Malha finha2");
        PROXIMOSETOR(now, 0);
    }

    if(now.hour() == BOMBAS[0].HORA_FIM && now.minute() == BOMBAS[0].MINUTO_FIM && BOMBAS[0].BOMBA_ATIVADA  == true ){
        
        if(BOMBAS[0].IRRIGACAO_PARCIAL_TEMPO){
            Serial.println("DEI MIGUÉ");
            IRRIGACAO(0);
            BOMBAS[0].SETOR_ANTERIOR = true;

        }else{
            Serial.println("EITZAAAA");
            //irrigar proximo setor dentro da hora de funcionamento
            if(PROXIMOSETOR(now, 0) && BOMBAS[0].CONTINUACAO_IRRIGACAO == false){
                Serial.println("DEI MIGUÉsw");
                IRRIGACAO(0);
                BOMBAS[0].SETOR_ANTERIOR = true;
            }
        }
    }

    
   
    if(now.hour() == BOMBAS[1].HORA_FIM && now.minute() == BOMBAS[1].MINUTO_FIM && BOMBAS[1].BOMBA_ATIVADA  == true ){
        
        if(BOMBAS[1].IRRIGACAO_PARCIAL_TEMPO){
            Serial.println("DEI MIGUÉ");
            IRRIGACAO(1);
            BOMBAS[1].SETOR_ANTERIOR = true;

        }else{
            Serial.println("passei aqui");
            //irrigar proximo setor dentro da hora de funcionamento
            if(PROXIMOSETOR(now, 1) && BOMBAS[1].CONTINUACAO_IRRIGACAO == false){
                Serial.println("DEI MIGUÉsw");
                IRRIGACAO(1);
                BOMBAS[1].SETOR_ANTERIOR = true;
            }
        }
    }

    if(now.hour() >= HORA_FUNCIONAMENTO[1] && BOMBAS[0].IRRIGACAO_ATIVA == true && BOMBAS[0].IRRIGACAO_PARCIAL_TEMPO == true){
        int hora = BOMBAS[0].HORA_FIM - now.hour();
        BOMBAS[0].HORA_RESTANTE = hora;
    }

    if(now.hour() >= HORA_FUNCIONAMENTO[1] && BOMBAS[1].IRRIGACAO_ATIVA == true && BOMBAS[1].IRRIGACAO_PARCIAL_TEMPO == true){
        int hora = BOMBAS[1].HORA_FIM - now.hour();
        BOMBAS[1].HORA_RESTANTE = hora;
    }

    if(now.minute() == segundo+1){
        //REGISTRO_IRRIGACAO('IRRIGANDO SETOR ', now);
        //SERIAL_TIME(now);
    }
    
    if(now.minute() == 59){
        segundo =- 1;
    }
   
}


void SERIAL_TIME(DateTime now){
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    segundo = now.minute();

    Serial.print("BOMBA 0:  H: ");
    Serial.print(BOMBAS[0].HORA_FIM_ANTERIOR);
    Serial.print("  |  M: ");
    Serial.print(BOMBAS[0].MINUTO_FIM_ANTERIOR);
    Serial.print("  |  I: ");
    Serial.print(BOMBAS[0].BOMBA_ATIVADA);
    Serial.print("  |  S: ");
    Serial.println(BOMBAS[0].SETOR_ANTERIOR);

    Serial.print("BOMBA 1:  H: ");
    Serial.print(BOMBAS[1].HORA_FIM_ANTERIOR);
    Serial.print("  |  M: ");
    Serial.print(BOMBAS[1].MINUTO_FIM_ANTERIOR);
    Serial.print("  |  I: ");
    Serial.print(BOMBAS[1].BOMBA_ATIVADA);
    Serial.print("  |  S: ");
    Serial.println(BOMBAS[1].SETOR_ANTERIOR);

    Serial.print("B1: ");
    Serial.print(REGISTRO[0]);
    Serial.print(" | B2: ");
    Serial.print(REGISTRO[1]);
    Serial.print(" | SA: ");
    Serial.print(REGISTRO[2]);
    Serial.print(" | SB: ");
    Serial.print(REGISTRO[3]);
    Serial.print(" | SC: ");
    Serial.print(REGISTRO[4]);
    Serial.print(" | SD: ");
    Serial.print(REGISTRO[5]);
    Serial.print(" | SE: ");
    Serial.print(REGISTRO[6]);
    Serial.print(" | SF: ");
    Serial.println(REGISTRO[7]);

}


void REGISTRO_IRRIGACAO(){

    DateTime now = rtc.now();
    
    Serial.println("Função REGISTRO_IRRIGAÇÃO  |  ");

    //gerarLog();


    //ENVIA_RADIO(ATIVIDADE,ESTADO);
    ENVIA_RADIO(BOMBAS[0]);
    delay(250);
    ENVIA_RADIO(BOMBAS[1]);

}

void ENVIA_RADIO(GRUPO BOMBAS){

    SET_ENVIA_RADIO();

    bool TX_CONFIRMA;
    TX_CONFIRMA = radio.write(&BOMBAS, sizeof(BOMBAS));
    

    if (TX_CONFIRMA) {
        if (radio.isAckPayloadAvailable()) {
            
            // read ack payload and copy data to relevant remoteNodeData array
            //radio.read(&remoteNodeData, sizeof(remoteNodeData));
            
            Serial.println("[+] Successfully received data from node: ");
        }
        
    }
    else {
        Serial.println("[-] The transmission to the selected node failed.");
        //salvarDados();
        //dados_buffer++;
    }
     
    

    SET_LER_RADIO();
}


void DEFINIR_TEMPO_IRRIGACAO(int SETOR, DateTime INICIO, int FUNC, int GRUPO){

    Serial.println("Função DEFINIR_TEMPO_IRRIGAÇÃO  |  ");
    registrarLog("Função DEFINIR_TEMPO_IRRIGAÇÃO chamada.");

    int minutos_inicio = INICIO.minute();
    int hora_inicio = INICIO.hour();
    int minutos_final = 0;
    int hora_final = 0;

    BOMBAS[GRUPO].INICIO_SETORES[BOMBAS[GRUPO].SETOR_VEZ_BOMBA] = INICIO;

     
    // ACIONA O TEMPO DE IRRIGAÇÃO DO SETOR DA VEZ
    if(FUNC == 0){
        int minutos_irrigados = BOMBAS[GRUPO].MINUTOS_SETOR[BOMBAS[GRUPO].SETOR_VEZ_BOMBA];
        minutos_final = minutos_inicio + minutos_irrigados;
    }

    if(FUNC == 2){
        minutos_final = minutos_inicio+2;
    }
    
 
    // DETERMINA O TEMPO DE DESATIVAÇÃO DO SETOR ANTERIOR
    else if(FUNC == 1){
        minutos_final = minutos_inicio + BOMBAS[GRUPO].INTERVALO_SETORES;
    }

    if(minutos_final >= 60){

        int hora_adicional = 0;
        while(true){   
            if(minutos_final-60 >= 0 ){
                minutos_final = minutos_final-60;
                hora_adicional += 1;
            }else{
                if(hora_adicional == 0){
                    hora_final = hora_inicio;
                    break;
                }
                hora_final = hora_inicio + hora_adicional;
                break;
            }
        }
    }else{
        hora_final = hora_inicio;
    }
    if(hora_final >= 24){
        hora_final = 0;
    }

    if(FUNC == 0){

        BOMBAS[GRUPO].HORA_FIM = hora_final;
        BOMBAS[GRUPO].MINUTO_FIM = minutos_final;
        registrarLog("Ação de irrigação definida.");
        registrarLog("COMANDO: " + String(SETOR) + "   HORA: " + String(hora_final) + " MINUTOS: " + String(minutos_final));
        Serial.print("COMANDO: ");
        Serial.print(SETOR);
        Serial.print("   HORA: ");
        Serial.print(hora_final);
        Serial.print(" MINUTOS: ");
        Serial.println(minutos_final);
    }

    if(FUNC == 1){

        BOMBAS[GRUPO].HORA_FIM_ANTERIOR = hora_final;
        BOMBAS[GRUPO].MINUTO_FIM_ANTERIOR = minutos_final;
        BOMBAS[GRUPO].SETOR_ANTERIOR == true;
        registrarLog("Tempo de desativação do setor anterior definido.");
        registrarLog("COMANDO: " + String(SETOR) + "   HORA: " + String(hora_final) + " MINUTOS: " + String(minutos_final));
    }  

    if(FUNC == 2){
        BOMBAS[GRUPO].HORA_INICIO = hora_final;
        BOMBAS[GRUPO].MINUTO_INICIO = minutos_final;
        BOMBAS[GRUPO].INICIO_SETUP_IRRIGACAO_BOMBA = false;
    }
}

void ATIVA_CARGA(const int CARGA, bool ESTADO, int GRUPO){
/*
    Serial.print("Função ATIVA_CARGA  |  ");
    Serial.print(CARGA);
    Serial.print("  Estado:  ");
    Serial.println(ESTADO);
*/
    // Registra a ação da função
    registrarLog("Função ATIVA_CARGA chamada.");
    registrarLog("Carga: " + String(CARGA) + " | Estado: " + String(ESTADO));
    
    //REGISTRO_IRRIGACAO(); 
    
    
    digitalWrite(CARGA, ESTADO);
    /*if(dados_buffer> 0){
         Serial.println("REsgatar");
        resgatarEDispararDados();
        dados_buffer = 0;
    }*/
}

void SETORES_ATIVOS(int GRUPO){

    Serial.println("Função SETORES_ATIVOS  |  ");
    registrarLog("Função SETORES_ATIVOS chamada para o grupo: " + String(GRUPO));

   // int contador_setor = 0;
    BOMBAS[GRUPO].SETOR_VEZ_BOMBA = 0;

     for(int i = 0; i < BOMBAS[GRUPO].QUANTIDADE_SETORES_BOMBA; i++){
        //Serial.println(SETOR_ATIVO[i]);
        delay(50);

        if(BOMBAS[GRUPO].SETOR_ATIVO[i] == 1 ){
            Serial.println(BOMBAS[GRUPO].SETORES[i]);
            //SETORES_HABILITADOS[contador_setor]=SETORES[i];
            //contador_setor += 1;
            registrarLog("Setor ativo: " + String(BOMBAS[GRUPO].SETORES[i]));
        }else {
            // Condição para registrar um erro se um setor não estiver ativo
            registrarErro("Setor inativo: " + String(BOMBAS[GRUPO].SETORES[i]));
        }
     }

     //QUANTIDADE_SETORES = contador_setor;
}

void INCIA_IRRIGACAO(int GRUPO){

    Serial.println("Função INICIA_IRRIGAÇÃO  |  ");
    registrarLog("Função INICIA_IRRIGAÇÃO chamada para o grupo: " + String(GRUPO));

    //ZERA SETOR DA VEZ E ATIVA VARIAVEL QUE SINALIZA IRRIGANDO
    BOMBAS[GRUPO].SETOR_VEZ_BOMBA = 0;
    BOMBAS[GRUPO].INICIO_SETUP_IRRIGACAO_BOMBA = true;

    //ACIONA SETOR E BOMBA
    ATIVA_CARGA(BOMBAS[GRUPO].SETORES[0], LOW, GRUPO);
    delay(2000);
    ATIVA_CARGA(BOMBAS[GRUPO].BOMBA,LOW, GRUPO);

    BOMBAS[GRUPO].INICIO_IRRIGACAO = rtc.now();
    BOMBAS[GRUPO].HORA_INICIO_IRRIGACAO = rtc.now();

    //REGISTRO_IRRIGACAO(7,1); // 7->inicia irrigação | 1->true
    
    //ATIVA VARIAVÉL QUE SINALIZA A BOMBA
    BOMBAS[GRUPO].BOMBA_ATIVADA = true;
    
    //DEFINE O TEMPO DO PROXIMO SETOR
    DEFINIR_TEMPO_IRRIGACAO(BOMBAS[GRUPO].SETORES[0], BOMBAS[GRUPO].INICIO_IRRIGACAO,0,GRUPO);  //SETOR E HORA DE INICIO

    BOMBAS[GRUPO].INICIO_SETUP_IRRIGACAO_BOMBA = false;

    BOMBAS[GRUPO].IRRIGACAO_INICIADA = true;
    registrarLog("Irrigação iniciada para o grupo: " + String(GRUPO));

    REGISTRO_IRRIGACAO();
}

void FINALIZA_IRRIGACAO(int GRUPO){

    Serial.println("Função FINALIZA_IRRIGAÇÃO  |  ");
    registrarLog("Função FINALIZA_IRRIGAÇÃO chamada para o grupo: " + String(GRUPO));

    ATIVA_CARGA(BOMBAS[GRUPO].BOMBA, HIGH, GRUPO);
    
    DateTime agora;
    BOMBAS[GRUPO].BOMBA_ATIVADA = false;
    BOMBAS[GRUPO].INICIO_SETORES[BOMBAS[GRUPO].SETOR_VEZ_BOMBA] = agora;
    //BOMBAS[GRUPO].HORA_INICIO = HORA_FUNCIONAMENTO[0];
    //BOMBAS[GRUPO].MINUTO_INICIO = 0;
    

    for(int i = 0; i < BOMBAS[GRUPO].QUANTIDADE_SETORES_BOMBA; i++){
        //SETOR_EM_ATIVIDADE[i] = true;
        ATIVA_CARGA(BOMBAS[GRUPO].SETORES[i], HIGH, GRUPO);
        delay(500);
        Serial.print("F: | ");
        Serial.println(BOMBAS[GRUPO].SETORES[i]);
    }
    
    DateTime HORA_FIM_IRRIGACAO = rtc.now();
    
    BOMBAS[GRUPO].HORA_INICIO = HORA_FUNCIONAMENTO[0];
    BOMBAS[GRUPO].MINUTO_INICIO = 0;
    Serial.println(BOMBAS[GRUPO].BOMBA_ATIVADA);
    BOMBAS[GRUPO].FIM_IRRIGACAO = HORA_FIM_IRRIGACAO;
    //DEFINIR_TEMPO_IRRIGACAO(BOMBAS[GRUPO].SETOR_VEZ_BOMBA, FIM_IRRIGACAO,2, GRUPO); 
    registrarLog("Irrigação finalizada para o grupo: " + String(GRUPO) + " às " + String(HORA_FIM_IRRIGACAO.hour()) + ":" + String(HORA_FIM_IRRIGACAO.minute()));

    REGISTRO_IRRIGACAO();
}


void ENCERRA_SETOR_ANTERIOR(int GRUPO){

    //Serial.print("Função ENCERRA_SETOR_ANTERIOR  |  ");
    registrarLog("Função ENCERRA_SETOR_ANTERIOR chamada para o grupo: " + String(GRUPO));

    BOMBAS[GRUPO].TROCA_DE_SETOR = false;
    //Serial.println("DESLIGANDO:  ");
    //Serial.println(SETORES_HABILITADOS[SETOR_VEZ]);

    ATIVA_CARGA(BOMBAS[GRUPO].SETORES[BOMBAS[GRUPO].SETOR_VEZ_BOMBA-1], HIGH, GRUPO);
    BOMBAS[GRUPO].SETOR_ANTERIOR = false;
    //SETOR_EM_ATIVIDADE[SETOR_VEZ-1] = true;

    REGISTRO_IRRIGACAO();
}


void IRRIGACAO(int GRUPO){

    Serial.println("Função IRRIGAÇÃO  |  ");
    registrarLog("Função IRRIGAÇÃO chamada para o grupo: " + String(GRUPO));

    //Serial.println(BOMBAS[GRUPO].SETOR_VEZ_BOMBA);
    //Serial.println(BOMBAS[GRUPO].QUANTIDADE_SETORES_BOMBA);
    Serial.println(BOMBAS[GRUPO].IRRIGACAO_PARCIAL_TEMPO);
    Serial.println(BOMBAS[GRUPO].BOMBA_ATIVADA);

    
    
    BOMBAS[GRUPO].SETOR_VEZ_BOMBA +=1;
    BOMBAS[GRUPO].INICIO_IRRIGACAO = rtc.now();

    if(BOMBAS[GRUPO].SETOR_VEZ_BOMBA < BOMBAS[GRUPO].QUANTIDADE_SETORES_BOMBA){
        registrarLog("Setor da vez anterior ao setor final: ");
        Serial.print("IRRIGANDO:  ");
        Serial.println(BOMBAS[GRUPO].SETORES[BOMBAS[GRUPO].SETOR_VEZ_BOMBA]);

        ATIVA_CARGA(BOMBAS[GRUPO].SETORES[BOMBAS[GRUPO].SETOR_VEZ_BOMBA], LOW, GRUPO);
        DEFINIR_TEMPO_IRRIGACAO(BOMBAS[GRUPO].SETOR_VEZ_BOMBA, BOMBAS[GRUPO].INICIO_IRRIGACAO,0, GRUPO);          //define o tempo do proximo setor
        BOMBAS[GRUPO].TROCA_DE_SETOR = true;
        DEFINIR_TEMPO_IRRIGACAO(BOMBAS[GRUPO].SETOR_VEZ_BOMBA-1, BOMBAS[GRUPO].INICIO_IRRIGACAO,1,GRUPO);        //define o tempo do setor anterior
        
        IRRIGADOS[GRUPO] = 1;
        //SETOR_EM_ATIVIDADE[SETOR_VEZ] = 0;

        REGISTRO_IRRIGACAO();
    }else if(BOMBAS[GRUPO].SETOR_VEZ_BOMBA == BOMBAS[GRUPO].QUANTIDADE_SETORES_BOMBA && !BOMBAS[GRUPO].CONTINUACAO_IRRIGACAO){
        BOMBAS[GRUPO].IRRIGACAO_INICIADA = false;
        registrarLog("Setor da vez é o ultimo setor.");
        FINALIZA_IRRIGACAO(GRUPO);
    }

    if(BOMBAS[GRUPO].BOMBA_ATIVADA  == false){
        Serial.println("Função ligando bomba");
        registrarLog("Bomba desligada, ligando bomba: " + String(BOMBAS[GRUPO].BOMBA));
        delay(1000);
        ATIVA_CARGA(BOMBAS[GRUPO].BOMBA,LOW, GRUPO);
        BOMBAS[GRUPO].BOMBA_ATIVADA = true;
    }

    
    
}

// Função para verificar se há tempo suficiente para iniciar um setor
bool PROXIMOSETOR(DateTime agora, int GRUPO) {
    /*Serial.print(": ");
    Serial.print(BOMBAS[GRUPO].SETOR_VEZ_BOMBA);
    Serial.print(" : ");
    Serial.println(BOMBAS[GRUPO].QUANTIDADE_SETORES_BOMBA);
*/
    if(BOMBAS[GRUPO].SETOR_VEZ_BOMBA == BOMBAS[GRUPO].QUANTIDADE_SETORES_BOMBA-1){
        registrarLog("Finalizando irrigação, último setor alcançado para o grupo: " + String(GRUPO));
        Serial.print("VIXEE TO AQUI ");
        BOMBAS[GRUPO].CONTINUACAO_IRRIGACAO = false;
        BOMBAS[GRUPO].IRRIGACAO_INICIADA = false;
        BOMBAS[GRUPO].HORA_INICIO = HORA_FUNCIONAMENTO[0];
        FINALIZA_IRRIGACAO(GRUPO);
        return false;
    }

    int hora_irrigacao = BOMBAS[GRUPO].MINUTOS_SETOR[BOMBAS[GRUPO].SETOR_VEZ_BOMBA];

    int horas = hora_irrigacao / 60;
    int minutos = hora_irrigacao % 60;
    
    Serial.print("Hora próximo Setor: ");
    Serial.print(agora.hour() + horas);
    Serial.print(" : ");
    Serial.println(agora.minute() + minutos);


    //SALVAR TEMPO RESTANTE DA IRRIGAÇÃO
    if(BOMBAS[GRUPO].IRRIGACAO_PARCIAL_TEMPO){
        Serial.println("ficou pra amanha vixe");
        int hora_restante = BOMBAS[GRUPO].HORA_FIM - HORA_FUNCIONAMENTO[1];

        BOMBAS[GRUPO].HORA_INICIO = HORA_FUNCIONAMENTO[0];
        BOMBAS[GRUPO].MINUTO_INICIO = 0;
        BOMBAS[GRUPO].HORA_FIM = hora_restante;
        BOMBAS[GRUPO].CONTINUACAO_IRRIGACAO = true;
        BOMBAS[GRUPO].SETOR_ANTERIOR = false;
        registrarLog("Irrigação parcial salva para o grupo: " + String(GRUPO));
        FINALIZA_IRRIGACAO(GRUPO);
    }

    //MINUTOS ESTÁ PARA TESTE TROCAR POR HORA
    if(agora.hour() + horas <= HORA_FUNCIONAMENTO[1]){ //DENTRO DA HORA DE FUNCIONAMENTO 
        registrarLog("Irrigação dentro da hora de funcionamento.");
        return true;
    } else{
        registrarLog("Irrigação programada para amanhã para o grupo: " + String(GRUPO));
        Serial.println("ficou pra amanha");

        BOMBAS[GRUPO].HORA_FIM = HORA_FUNCIONAMENTO[0] + horas;
        BOMBAS[GRUPO].MINUTO_FIM = minutos;
        BOMBAS[GRUPO].CONTINUACAO_IRRIGACAO = true;
        BOMBAS[GRUPO].SETOR_ANTERIOR = false;
        
        FINALIZA_IRRIGACAO(GRUPO);
    }


}

bool PROXIMO_SETO( DateTime hoje, int GRUPO, int setor){

    int hora_agora = BOMBAS[GRUPO].MINUTOS_SETOR[BOMBAS[GRUPO].SETOR_VEZ_BOMBA]/60;

    if(hoje.minute()+ hora_agora < HORA_FUNCIONAMENTO[0]){
        return false;
    }
}

void CALCULA_HORA_RESTANTE(DateTime now, int GRUPO){
    int hora = BOMBAS[GRUPO].HORA_FIM - now.hour();
    BOMBAS[GRUPO].HORA_RESTANTE = HORA_FUNCIONAMENTO[0] + hora;
}

void CONFIGURE_SETUP_REMOTO(){

    SET_ENVIA_RADIO();

    radio.write(&indica_inicio, sizeof(indica_inicio));

    delay(100);

    SET_LER_RADIO();
    
    int data_setup;
    int funcao_comando = 0;
    int posicao = 0;

    while(funcao_comando >= 0){
        if (radio.available()) {
            radio.read(&funcao_comando, sizeof(funcao_comando));
            break;
        }
    }



    while(data_setup != 200){

        if (radio.available()) {
        
        radio.read(&data_setup, sizeof(data_setup));
        
        //Serial.print(data_setup);
        //Serial.print(" | ");

        if(data_setup == -2){
            funcao_comando = -2;
        }

        if(funcao_comando == -1){
            //DEFINE SETORES DA BOMBA1
            if(data_setup == 300){
                funcao_comando = data_setup;
                posicao = 0;
            }
            //DEFINE SETORES DA BOMBA2
            else if(data_setup == 301){
                funcao_comando = data_setup;
                posicao = 0;
            }
            //DEFINE SETORES ATIVOS DA BOMBA1
            else if(data_setup == 302){
                funcao_comando = data_setup;
                posicao = 0;
            }
            //DEFINE SETORES ATIVOS DA BOMBA2
            else if(data_setup == 303){
                funcao_comando = data_setup;
                posicao = 0;
            }
            //DEFINE MINUTOS POR SETORES DA BOMBA2
            else if(data_setup == 304){
                funcao_comando = data_setup;
                posicao = 0;
            }
            //DEFINE MINUTOS POR SETORES DA BOMBA2
            else if(data_setup == 305){
                funcao_comando = data_setup;
                posicao = 0;
            }
        } else if(funcao_comando > 0){

            if(funcao_comando == 300){
                int setor_envio = SETOR_CORRESPODENTE(data_setup);
                ADICIONA_SETOR_GRUPO_BOMBA(0, setor_envio, posicao);
                posicao++;
            }

            else if(funcao_comando == 301){
                int setor_envio = SETOR_CORRESPODENTE(data_setup);
                ADICIONA_SETOR_GRUPO_BOMBA(1, setor_envio, posicao);
                posicao++;
            }
        }

        if(funcao_comando == -2){

            Serial.print("Posi: ");
            Serial.print(posicao);

            Serial.print(" data: ");
            Serial.println(data_setup);

            Serial.println(BOMBAS[0].MINUTOS_SETOR[posicao]);
            Serial.println(BOMBAS[1].MINUTOS_SETOR[posicao]);

            BOMBAS[0].MINUTOS_SETOR[posicao] = data_setup;
            BOMBAS[1].MINUTOS_SETOR[posicao] = data_setup;
            
            Serial.println(BOMBAS[0].MINUTOS_SETOR[posicao]);
            Serial.println(BOMBAS[1].MINUTOS_SETOR[posicao]);
            posicao++;
        }
       
        


        //COMANDO ENCERRANDO TRANSMISSÃO DE DADOS
        if(funcao_comando == 200){
            Serial.println(".");
            break;
        }

        }
    }

}

void ENVIA_STATUS_ATUAL(){
    SET_ENVIA_RADIO();
    for(int i = 0; i<2; i++){
        radio.write(&BOMBAS[i].TROCA_DE_SETOR, sizeof(BOMBAS[i].TROCA_DE_SETOR));
        delay(100);
        radio.write(&BOMBAS[i].QUANTIDADE_SETORES_BOMBA, sizeof(BOMBAS[i].QUANTIDADE_SETORES_BOMBA));
        delay(100);
        radio.write(&BOMBAS[i].SETOR_VEZ_BOMBA, sizeof(BOMBAS[i].SETOR_VEZ_BOMBA));
        delay(100);
        radio.write(&BOMBAS[i].INICIO_SETUP_IRRIGACAO_BOMBA, sizeof(BOMBAS[i].INICIO_SETUP_IRRIGACAO_BOMBA));
        delay(100);
        radio.write(&BOMBAS[i].BOMBA_ATIVADA, sizeof(BOMBAS[i].BOMBA_ATIVADA));
        delay(100);
        radio.write(&BOMBAS[i].BOMBA, sizeof(BOMBAS[i].BOMBA));
        delay(100);
        radio.write(&BOMBAS[i].SETOR_ANTERIOR, sizeof(BOMBAS[i].SETOR_ANTERIOR));
        delay(100);
        radio.write(&BOMBAS[i].HORA_INICIO, sizeof(BOMBAS[i].HORA_INICIO));
        delay(100);
        radio.write(&BOMBAS[i].MINUTO_INICIO, sizeof(BOMBAS[i].MINUTO_INICIO));
        delay(100);
        radio.write(&BOMBAS[i].HORA_FIM, sizeof(BOMBAS[i].HORA_FIM));
        delay(100);
        radio.write(&BOMBAS[i].MINUTO_FIM, sizeof(BOMBAS[i].MINUTO_FIM));
        delay(100);
        radio.write(&BOMBAS[i].HORA_FIM_ANTERIOR, sizeof(BOMBAS[i].HORA_FIM_ANTERIOR));
        delay(100);
        radio.write(&BOMBAS[i].MINUTO_FIM_ANTERIOR, sizeof(BOMBAS[i].MINUTO_FIM_ANTERIOR));
        delay(100);
        radio.write(&BOMBAS[i].INTERVALO_SETORES, sizeof(BOMBAS[i].INTERVALO_SETORES));
        delay(100);
        radio.write(&BOMBAS[i].HORA, sizeof(BOMBAS[i].HORA));
        delay(100);
        radio.write(&BOMBAS[i].MINUTO, sizeof(BOMBAS[i].MINUTO));
        delay(100);
        int fim = 201;
        radio.write(&fim, sizeof(fim));
    }
    int fim = 200;
    radio.write(&fim, sizeof(fim));
    SET_LER_RADIO();
}

int SETOR_CORRESPODENTE(int SETOR){

    if(SETOR == 0){
        return SOLENOIDE_A;
    }else if(SETOR == 1){
        return SOLENOIDE_B;
    }else if(SETOR == 2){
        return SOLENOIDE_C;
    }else if(SETOR == 3){
        return SOLENOIDE_D;
    }else if(SETOR == 4){
        return SOLENOIDE_E;
    }else if(SETOR == 5){
        return SOLENOIDE_F;
    }

}

void ADICIONA_SETOR_GRUPO_BOMBA(int GRUPO, int SETOR, int POSICAO){
    BOMBAS[GRUPO].SETORES[POSICAO] = SETOR;
    Serial.println(BOMBAS[GRUPO].SETORES[POSICAO]);
}

void STATUS_SERIAL(int i){
        Serial.print("TRO.SET: ");
        Serial.print(BOMBAS[i].TROCA_DE_SETOR);
        Serial.print(" | QTD.SET: ");
        Serial.print(BOMBAS[i].QUANTIDADE_SETORES_BOMBA);
        Serial.print(" | SET.VEZ: ");
        Serial.print(BOMBAS[i].SETOR_VEZ_BOMBA);
        Serial.print(" | INC.IRR: ");
        Serial.println(BOMBAS[i].INICIO_SETUP_IRRIGACAO_BOMBA);
        Serial.print(" | BOM.ATV: ");
        Serial.print(BOMBAS[i].BOMBA_ATIVADA);
        Serial.print(" | BOMBA: ");
        Serial.print(BOMBAS[i].BOMBA);
        Serial.print(" | SET.ANT: ");
        Serial.println(BOMBAS[i].SETOR_ANTERIOR);
        Serial.print(" | HOR.INI: ");
        Serial.print(BOMBAS[i].HORA_INICIO);
        Serial.print(" | MIN.INI: ");
        Serial.print(BOMBAS[i].MINUTO_INICIO);
        Serial.print(" | HOR.FIM: ");
        Serial.print(BOMBAS[i].HORA_FIM);
        Serial.print(" | MIN.FIM: ");
        Serial.println(BOMBAS[i].MINUTO_FIM);
        Serial.print(" | HFIM.ANT: ");
        Serial.print(BOMBAS[i].HORA_FIM_ANTERIOR);
        Serial.print(" | MFIM.ANT: ");
        Serial.print(BOMBAS[i].MINUTO_FIM_ANTERIOR);
        Serial.print(" | INT.SET: ");
        Serial.print(BOMBAS[i].INTERVALO_SETORES);
        Serial.print(" | HORA: ");
        Serial.print(BOMBAS[i].HORA);
        Serial.print(" | MIN: ");
        Serial.println(BOMBAS[i].MINUTO);
}

void exibirTamanhosArquivos() {
    // Abre o arquivo para leitura
    File logFile = SPIFFS.open("/log.txt", FILE_READ);
    if (!logFile) {
        Serial.println("Falha ao abrir o arquivo para leitura.");
        registrarErro("Falha ao abrir o arquivo para leitura.");
        return;
    }

    // Obtém o tamanho do arquivo
    size_t tamanhoLog = logFile.size();
    Serial.print("Tamanho do arquivo de log: ");
    Serial.print(tamanhoLog);
    Serial.println(" bytes");

    // Fecha o arquivo
    logFile.close();
}

void gerarLog() {
    Serial.println("===== LOG DO SISTEMA =====");

    // Exibe o estado de cada variável do grupo de bombas
    for (int i = 0; i < 2; i++) {
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

    // Exibe o estado geral das variáveis globais
    Serial.println("Variáveis globais:");
    Serial.print("Setor em atividade: ");
    Serial.print("Posição do buffer: ");
    Serial.println(POSICAO_BUFFER);


    Serial.println("==========================");
}

void printDateTime(DateTime dataHora) {
    char buffer[20];  // Buffer para armazenar a data/hora formatada

    // Formata a data e hora no estilo "DD/MM/YYYY HH:MM:SS"
    sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", 
            dataHora.day(), dataHora.month(), dataHora.year(), 
            dataHora.hour(), dataHora.minute(), dataHora.second());

    Serial.println(buffer);  // Exibe a data/hora formatada no Serial
}

// Grava a hora de início de cada setor (hora e minuto)
void GRAVAR_HORA(int setor, DateTime inicio) {
    int endereco = setor * 2;  // Cada setor usa 2 bytes (hora + minuto)

    //EEPROM.write(endereco, inicio.hour());       // Grava a hora
    //EEPROM.write(endereco + 1, inicio.minute()); // Grava o minuto
    EEPROM.commit();  // Confirma a gravação na flash
    Serial.print("Gravado setor ");
    Serial.print(setor);
    Serial.print(" - Hora: ");
    Serial.print(inicio.hour());
    Serial.print(":");
    Serial.println(inicio.minute());
}

// Lê o horário de início de um setor e retorna um objeto DateTime
DateTime lerHorarioSetor(int setor) {
    int endereco = setor * 2;  // Cada setor usa 2 bytes

    int hora = EEPROM.read(endereco);       // Lê a hora
    int minuto = EEPROM.read(endereco + 1); // Lê o minuto

    Serial.print("Lido setor ");
    Serial.print(setor);
    Serial.print(" - Hora: ");
    Serial.print(hora);
    Serial.print(":");
    Serial.println(minuto);

    // Retorna o horário como um objeto DateTime (com data fictícia)
    return DateTime(2024, 10, 17, hora, minuto, 0);
}

// Função para gravar horários para todos os setores de irrigação
void gravarTodosSetores(DateTime horarios[]) {
    for (int i = 0; i < NUM_SETORES; i++) {
        GRAVAR_HORA(i, horarios[i]);
    }
}

// Função para ler os horários de todos os setores e mostrar no Serial
void lerTodosSetores() {
    for (int i = 0; i < NUM_SETORES; i++) {
        DateTime horario = lerHorarioSetor(i);  // Lê cada setor
    }
}

// Função para registrar logs em arquivo
void registrarLog(String mensagem) {
    DateTime agora = rtc.now();
    /*File logFile = SPIFFS.open("/log.txt", FILE_APPEND); // Abre o arquivo em modo de adição

    if (!logFile) {
        Serial.println("Erro ao abrir o arquivo de log.");
        return;
    }

    // Montando o log com timestamp
    logFile.print("[LOG] ");
    logFile.print(agora.timestamp());  //DateTime::TIMESTAMP_TIME
    logFile.print(" - ");
    logFile.println(mensagem);
    logFile.close();  // Fechando o arquivo*/
}

// Função para registrar erros
void registrarErro(String erro) {
    DateTime agora = rtc.now();
    File logFile = SPIFFS.open("/log.txt", FILE_APPEND);

    if (!logFile) {
        Serial.println("Erro ao abrir o arquivo de log.");
        return;
    }

    logFile.print("[ERRO] ");
    logFile.print(agora.timestamp(DateTime::TIMESTAMP_TIME));
    logFile.print(" - ");
    logFile.println(erro);
    logFile.close();
}

// Função para ler os logs do arquivo
void lerLogs() {
    File logFile = SPIFFS.open("/log.txt", FILE_READ);

    if (!logFile) {
        Serial.println("Erro ao abrir o arquivo de log.");
        return;
    }

    while (logFile.available()) {
        String linha = logFile.readStringUntil('\n');
        Serial.println(linha);  // Imprime no monitor serial
    }
    logFile.close();
}

void deletarTodosArquivos() {
    Serial.println("Deletando todos os arquivos...");

    // Abre um diretório para listar os arquivos
    File dir = SPIFFS.open("/");
    File file = dir.openNextFile();

    // Itera sobre todos os arquivos no diretório
    while (file) {
        String fileName = file.name();
        Serial.print("Deletando: ");
        Serial.println(fileName); // Mostra qual arquivo está sendo deletado
        
        if (SPIFFS.remove(fileName)) { // Remove o arquivo
            Serial.print("Arquivo deletado: ");
            Serial.println(fileName);
        } else {
            Serial.print("Erro ao deletar arquivo: ");
            Serial.println(fileName);
        }
        
        file = dir.openNextFile(); // Avança para o próximo arquivo
    }

    Serial.println("Todos os arquivos foram deletados.");
}

void salvarDados() {
    File file = SPIFFS.open("/dados.bin", FILE_WRITE);
    if (!file) {
        Serial.println("Erro ao abrir arquivo para escrita");
        return;
    }

    // Escrever os dados binários no arquivo
    for (int i = 0; i < 2; i++) {
        file.write((uint8_t*)&BOMBAS[i], sizeof(GRUPO));
    }

    file.close();
    Serial.println("Dados salvos no SPIFFS");
}

void exibirDados() {
    File file = SPIFFS.open("/dados.bin", FILE_READ);
    if (!file) {
        Serial.println("Erro ao abrir arquivo para leitura");
        return;
    }

    // Ler os dados binários e carregar nas estruturas
    for (int i = 0; i < 2; i++) {
        file.read((uint8_t*)&BOMBAS[i], sizeof(GRUPO));
        
        // Exibindo os valores da estrutura BOMBAS[i]
        Serial.println("Bomba " + String(i));

        // Variáveis simples
        Serial.print("QUANTIDADE_SETORES_BOMBA: ");
        Serial.println(BOMBAS[i].QUANTIDADE_SETORES_BOMBA);
        
        Serial.print("SETOR_VEZ_BOMBA: ");
        Serial.println(BOMBAS[i].SETOR_VEZ_BOMBA);
        
        Serial.print("BOMBA: ");
        Serial.println(BOMBAS[i].BOMBA);
        
        Serial.print("TROCA_DE_SETOR: ");
        Serial.println(BOMBAS[i].TROCA_DE_SETOR);
        
        Serial.print("INICIO_SETUP_IRRIGACAO_BOMBA: ");
        Serial.println(BOMBAS[i].INICIO_SETUP_IRRIGACAO_BOMBA);
        
        Serial.print("BOMBA_ATIVADA: ");
        Serial.println(BOMBAS[i].BOMBA_ATIVADA);

        Serial.print("SETOR_ANTERIOR: ");
        Serial.println(BOMBAS[i].SETOR_ANTERIOR);

        // Formatando e exibindo INICIO_IRRIGACAO, HORA_INICIO_IRRIGACAO, FIM_IRRIGACAO
        Serial.print("INICIO_IRRIGACAO: ");
        Serial.println(formatDateTime(BOMBAS[i].INICIO_IRRIGACAO));

        Serial.print("HORA_INICIO_IRRIGACAO: ");
        Serial.println(formatDateTime(BOMBAS[i].HORA_INICIO_IRRIGACAO));

        Serial.print("FIM_IRRIGACAO: ");
        Serial.println(formatDateTime(BOMBAS[i].FIM_IRRIGACAO));

        // Arrays de DateTime
        for (int j = 0; j < 6; j++) {
            Serial.print("INICIO_SETORES[" + String(j) + "]: ");
            Serial.println(formatDateTime(BOMBAS[i].INICIO_SETORES[j]));
        }

        // Variáveis de hora e minuto
        Serial.print("HORA_INICIO: ");
        Serial.println(BOMBAS[i].HORA_INICIO);
        
        Serial.print("MINUTO_INICIO: ");
        Serial.println(BOMBAS[i].MINUTO_INICIO);
        
        Serial.print("HORA_FIM: ");
        Serial.println(BOMBAS[i].HORA_FIM);
        
        Serial.print("MINUTO_FIM: ");
        Serial.println(BOMBAS[i].MINUTO_FIM);
        
        Serial.print("HORA_FIM_ANTERIOR: ");
        Serial.println(BOMBAS[i].HORA_FIM_ANTERIOR);
        
        Serial.print("MINUTO_FIM_ANTERIOR: ");
        Serial.println(BOMBAS[i].MINUTO_FIM_ANTERIOR);
        
        // Exibindo os arrays
        for (int j = 0; j < 6; j++) {
            Serial.print("SETORES[" + String(j) + "]: ");
            Serial.println(BOMBAS[i].SETORES[j]);
        }

        for (int j = 0; j < 6; j++) {
            Serial.print("SETOR_ATIVO[" + String(j) + "]: ");
            Serial.println(BOMBAS[i].SETOR_ATIVO[j]);
        }

        for (int j = 0; j < 6; j++) {
            Serial.print("MINUTOS_SETOR[" + String(j) + "]: ");
            Serial.println(BOMBAS[i].MINUTOS_SETOR[j]);
        }

        // Exibindo o controle de irrigação
        Serial.print("INTERVALO_SETORES: ");
        Serial.println(BOMBAS[i].INTERVALO_SETORES);
        
        Serial.print("HORA: ");
        Serial.println(BOMBAS[i].HORA);
        
        Serial.print("MINUTO: ");
        Serial.println(BOMBAS[i].MINUTO);

        Serial.print("IRRIGACAO_PARCIAL_TEMPO: ");
        Serial.println(BOMBAS[i].IRRIGACAO_PARCIAL_TEMPO);

        Serial.print("IRRIGACAO_INICIADA: ");
        Serial.println(BOMBAS[i].IRRIGACAO_INICIADA);

        Serial.print("CONTINUACAO_IRRIGACAO: ");
        Serial.println(BOMBAS[i].CONTINUACAO_IRRIGACAO);

        Serial.print("IRRIGACAO_ATIVA: ");
        Serial.println(BOMBAS[i].IRRIGACAO_ATIVA);

        Serial.print("HORA_PENDENTE: ");
        Serial.println(BOMBAS[i].HORA_PENDENTE);

        Serial.print("HORA_RESTANTE: ");
        Serial.println(BOMBAS[i].HORA_RESTANTE);

        Serial.println(); // Separando as bombas
    }

    file.close();
    Serial.println("Dados carregados e exibidos da SPIFFS");
}

String formatDateTime(DateTime dt) {
    String formattedDate = String(dt.day(), DEC) + "/" + String(dt.month(), DEC) + "/" + String(dt.year(), DEC) + " ";
    formattedDate += String(dt.hour(), DEC) + ":" + String(dt.minute(), DEC) + ":" + String(dt.second(), DEC);
    return formattedDate;
}

void resgatarEDispararDados() {
    // Abrir o arquivo da SPIFFS para leitura
    File file = SPIFFS.open("/dados.bin", FILE_READ); // Use o mesmo nome de arquivo que você usou para salvar os dados binários
    GRUPO bombas[2];  // Defina a estrutura GRUPO que será preenchida com os dados lidos

    if (!file) {
        Serial.println("Erro ao abrir o arquivo para leitura");
        return;
    }

    // Ler os dados do arquivo e armazenar na estrutura GRUPO
    int i = 0;
    while (file.available() && i < 2) {  // Lê até 2 bombas, ou até o final do arquivo
        file.read((uint8_t*)&bombas[i], sizeof(GRUPO)); // Deserializar os dados para a estrutura GRUPO
        i++;
    }

    // Fechar o arquivo após a leitura
    file.close();

    // Enviar os dados lidos através do NRF
    for (int j = 0; j < i; j++) {  // Enviar as bombas carregadas
        ENVIA_RADIO(bombas[j]);
        delay(250);  // Delay entre os envios
    }

    // Limpar a SPIFFS após o envio bem-sucedido
    SPIFFS.remove("/dados.bin");
    Serial.println("[+] Dados da SPIFFS removidos após envio.");
}
