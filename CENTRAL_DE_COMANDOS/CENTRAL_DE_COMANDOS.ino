#include <RF24.h>

#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Firebase esp Client 2.3.7

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

//======================================== Insert your network credentials.
const char* WIFI_SSID = "ATUALIZE_INTELBRASS";
const char* WIFI_PASSWORD = "12345678";
//======================================== 
WiFiServer server(80);

// Insert Firebase project API Key
#define API_KEY "AIzaSyAinDXxolmbXRbQ9a_lRyKwLMNrtFxk4R8"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://ainsof-arian-default-rtdb.firebaseio.com/" 

// Define Firebase Data object.
FirebaseData fbdo;

// Define firebase authentication.
FirebaseAuth auth;

// Definee firebase configuration.
FirebaseConfig config;

#define USER_EMAIL "aryan@gmail.com"
#define USER_PASSWORD "123456"

float store_random_Float_Val;
int store_random_Int_Val;

RF24 radio(4, 5); // Pinos CE, CSN
const uint64_t address = 0xF0F0F0F0F1LL;
int remoteNodeData[2] = {1, 1,};
String uid;

void CONFIG_FIREBASE(){

    config.api_key = API_KEY;  // Assign the api key (required).
    
    config.database_url = DATABASE_URL;  // Assign the RTDB URL (required).
    
    // Sign up.
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD; 

    config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
    config.max_token_generation_retry = 5;  // Assign the maximum retry of token generation

    Firebase.begin(&config, &auth);

    // Getting the user UID might take a few seconds
    Serial.println("Getting User UID");

    while ((auth.token.uid) == "") {
        Serial.print('.');
        delay(1000);
    }

    // Print user UID
    uid = auth.token.uid.c_str();
    Serial.print("User UID: ");
    Serial.print(uid);

    Firebase.reconnectWiFi(true);
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

void CONFIG_WIFI(){
    //WiFi.mode(WIFI_STA); 
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }
    server.begin();
    Serial.println(WiFi.localIP());    //---------------------------------------- 
}

void setup() {
    Serial.begin(115200);
    
    CONFIG_RADIO();
    CONFIG_WIFI();
    CONFIG_FIREBASE();
   
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


void acionamento_bomba(bool estado){
   Serial.println(estado);
   //radio.write(&estado, sizeof(estado));
}
void ENVIA_SETUP_CONFIGURACAO(int envio) {
    Serial.println(envio);
    SET_ENVIA_RADIO();
    
    radio.write(&envio, sizeof(envio));

    
       
}


void loop() {

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }
/*
    ENVIA_SETUP_CONFIGURACAO(1);

    SET_LER_RADIO();
    while(true){
        if (radio.available()) {
            int data;
            radio.read(&data, sizeof(data));
            Serial.println(data);
            

            if(data == 1001){
                ENVIA_SETUP_CONFIGURACAO(300);
                delay(200);
                for(int i =0; i<6; i++){
                    ENVIA_SETUP_CONFIGURACAO(i);
                }
                ENVIA_SETUP_CONFIGURACAO(200);
                break;
            }
        }
    }

    delay(1000);

*/
 
    
  
        
       /*
        delay(5000);
        radio.stopListening();  // Muda para o modo de transmissão
        radio.openWritingPipe(address);
        data += 1;
        radio.write(&data, sizeof(data));
        delay(1000);
         radio.openReadingPipe(0, address);
        radio.startListening();
   
    } */
    //delay(5000);
    //enviaFirebase();
  
}



void enviaFirebase(int ATIVIDADE, int ESTADO, int HORA, int MINUTO, int SEGUNDO){
        String texto;
        texto += HORA;
        texto += ':';
        texto += MINUTO;
        texto += ':';
        texto += SEGUNDO;
        Serial.println(texto);
    switch(ATIVIDADE){

        //INICIANDO IRRIGAÇÃO
        case 15:
            if (Firebase.RTDB.setInt(&fbdo, "BOMBA1/ESTADO", ESTADO)) {
                Serial.println("PASSED");
            }
            
            if (Firebase.RTDB.setString(&fbdo, "BOMBA1/HORA", texto)) {
                Serial.println("PASSED");
            }
            else {
                Serial.println("FAILED");
            }
            break;

        //INICIANDO IRRIGAÇÃO
        case 32:
            if (Firebase.RTDB.setInt(&fbdo, "BOMBA2/ESTADO", ESTADO)) {
                Serial.println("PASSED");
            }
            if (Firebase.RTDB.setString(&fbdo, "BOMBA2/HORA", texto)) {
                Serial.println("PASSED");
            }
            else {
                Serial.println("FAILED");
            }
            break;

        //INICIANDO IRRIGAÇÃO
        case 25:
            if (Firebase.RTDB.setInt(&fbdo, "SETOR A/ESTADO", ESTADO)) {
                Serial.println("PASSED");
            }
            if (Firebase.RTDB.setString(&fbdo, "SETOR A/HORA", texto)) {
                Serial.println("PASSED");
            }
            else {
                Serial.println("FAILED");
            }
            break;

        //INICIANDO IRRIGAÇÃO
        case 13:
            if (Firebase.RTDB.setInt(&fbdo, "SETOR B/ESTADO", ESTADO)) {
                Serial.println("PASSED");
            }
            if (Firebase.RTDB.setString(&fbdo, "SETOR B/HORA", texto)) {
                Serial.println("PASSED");
            }
            else {
                Serial.println("FAILED");
            }
            break;
        //INICIANDO IRRIGAÇÃO
        case 14:
            if (Firebase.RTDB.setInt(&fbdo, "SETOR C/ESTADO", ESTADO)) {
                Serial.println("PASSED");
            }
            if (Firebase.RTDB.setString(&fbdo, "SETOR C/HORA", texto)) {
                Serial.println("PASSED");
            }
            else {
                Serial.println("FAILED");
            }
            break;

        //INICIANDO IRRIGAÇÃO
        case 2:
            if (Firebase.RTDB.setInt(&fbdo, "SETOR D/ESTADO", ESTADO)) {
                Serial.println("PASSED");
            }
            if (Firebase.RTDB.setString(&fbdo, "SETOR D/HORA", texto)) {
                Serial.println("PASSED");
            }
            else {
                Serial.println("FAILED");
            }
            break;

        //INICIANDO IRRIGAÇÃO
        case 27:
            if (Firebase.RTDB.setInt(&fbdo, "SETOR E/ESTADO", ESTADO)) {
                Serial.println("PASSED");
            }
            if (Firebase.RTDB.setString(&fbdo, "SETOR E//HORA", texto)) {
                Serial.println("PASSED");
            }
            else {
                Serial.println("FAILED");
            }
            break;

        //INICIANDO IRRIGAÇÃO
        case 33:
            if (Firebase.RTDB.setInt(&fbdo, "SETOR F/ESTADO", ESTADO)) {
                Serial.println("PASSED");
            }
            if (Firebase.RTDB.setString(&fbdo, "SETOR F/HORA", texto)) {
                Serial.println("PASSED");
            }
            else {
                Serial.println("FAILED");
            }
            break;

        default:
            Serial.println("error case envio FIREBASE");
    }

    


    /*
    for(int i = 0; i < 12; i++){
        // Write an Float number on the database path test/random_Int_Val.
        
        if (Firebase.RTDB.setInt(&fbdo, texto[i], data[i])) {
        Serial.println("PASSED");
        Serial.println(data[i]);
        }
        else {
            Serial.println("FAILED");
        }
        delay(25);
        }*/
}

void enviarDadosFirebase(int grupoIndex) {
    // Caminho base para o grupo e chave única para cada dado
    String caminhoBase = "/Grupo/" + String(BOMBAS[grupoIndex].INICIO_IRRIGACAO.unixtime());

    // Enviar cada variável individualmente usando push
    Firebase.RTDB.pushInt(&fbdo, caminhoBase + "/QUANTIDADE_SETORES_BOMBA", BOMBAS[grupoIndex].QUANTIDADE_SETORES_BOMBA);
    Firebase.RTDB.pushInt(&fbdo, caminhoBase + "/SETOR_VEZ_BOMBA", BOMBAS[grupoIndex].SETOR_VEZ_BOMBA);
    Firebase.RTDB.pushInt(&fbdo, caminhoBase + "/BOMBA", BOMBAS[grupoIndex].BOMBA);
    Firebase.RTDB.pushBool(&fbdo, caminhoBase + "/TROCA_DE_SETOR", BOMBAS[grupoIndex].TROCA_DE_SETOR);
    Firebase.RTDB.pushBool(&fbdo, caminhoBase + "/INICIO_SETUP_IRRIGACAO_BOMBA", BOMBAS[grupoIndex].INICIO_SETUP_IRRIGACAO_BOMBA);
    Firebase.RTDB.pushBool(&fbdo, caminhoBase + "/BOMBA_ATIVADA", BOMBAS[grupoIndex].BOMBA_ATIVADA);

    // Converter DateTime para timestamp e enviar
    Firebase.RTDB.pushInt(&fbdo, caminhoBase + "/INICIO_IRRIGACAO", BOMBAS[grupoIndex].INICIO_IRRIGACAO.unixtime());
    Firebase.RTDB.pushInt(&fbdo, caminhoBase + "/HORA_INICIO", BOMBAS[grupoIndex].HORA_INICIO);
    Firebase.RTDB.pushInt(&fbdo, caminhoBase + "/MINUTO_INICIO", BOMBAS[grupoIndex].MINUTO_INICIO);
    Firebase.RTDB.pushInt(&fbdo, caminhoBase + "/HORA_FIM", BOMBAS[grupoIndex].HORA_FIM);
    Firebase.RTDB.pushInt(&fbdo, caminhoBase + "/MINUTO_FIM", BOMBAS[grupoIndex].MINUTO_FIM);

    Firebase.RTDB.pushBool(&fbdo, caminhoBase + "/IRRIGACAO_PARCIAL_TEMPO", BOMBAS[grupoIndex].IRRIGACAO_PARCIAL_TEMPO);
    Firebase.RTDB.pushBool(&fbdo, caminhoBase + "/IRRIGACAO_INICIADA", BOMBAS[grupoIndex].IRRIGACAO_INICIADA);
    Firebase.RTDB.pushBool(&fbdo, caminhoBase + "/CONTINUACAO_IRRIGACAO", BOMBAS[grupoIndex].CONTINUACAO_IRRIGACAO);
    Firebase.RTDB.pushBool(&fbdo, caminhoBase + "/IRRIGACAO_ATIVA", BOMBAS[grupoIndex].IRRIGACAO_ATIVA);
    Firebase.RTDB.pushBool(&fbdo, caminhoBase + "/HORA_PENDENTE", BOMBAS[grupoIndex].HORA_PENDENTE);
    Firebase.RTDB.pushInt(&fbdo, caminhoBase + "/HORA_RESTANTE", BOMBAS[grupoIndex].HORA_RESTANTE);

    Serial.println("Dados enviados para o Firebase com push.");
}