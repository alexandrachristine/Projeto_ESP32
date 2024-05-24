#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h> // Biblioteca para obter a data e hora

// Defina as credenciais do Wi-Fi
#define WIFI_SSID "TEVINHO_2G_5G"
#define WIFI_PASSWORD "Esteves@141121206"

// Defina a chave API do Firebase, ID do projeto e credenciais do usuário
#define API_KEY "AIzaSyC5FwacvpuxhbwAfmrfXocgDzgxwLuGFkk"
#define FIREBASE_PROJECT_ID "ocean-b05ce"
#define USER_EMAIL "hebertesteves14.sp@gmail.com"
#define USER_PASSWORD "hebert206"

// Defina o objeto de dados do Firebase, autenticação e configuração
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// GPIO onde o DS18B20 está conectado
const int oneWireBus = 4;

// Defina o pino para o sensor de água
int sensorPin = 32; // Use um pino do ADC1
int leitura; // Variável responsável por guardar o valor da leitura analógica do sensor
String condicao; // Variável para armazenar a condição da água

// Configurar uma instância OneWire para se comunicar com dispositivos OneWire
OneWire oneWire(oneWireBus);

// Passar nossa referência OneWire para o sensor de temperatura Dallas 
DallasTemperature sensors(&oneWire);

// Defina o fuso horário (no caso, UTC-3 para horário de Brasília)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

void setup() {
  // Inicialize a comunicação serial para depuração
  Serial.begin(115200);
  sensors.begin();

  // Conecte-se ao Wi-Fi
  connectWiFi();

  // Imprima a versão do cliente Firebase
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  // Atribua a chave API
  config.api_key = API_KEY;

  // Atribua as credenciais de login do usuário
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Atribua a função de callback para a tarefa de geração de token de longa duração
  config.token_status_callback = tokenStatusCallback;  // veja addons/TokenHelper.h

  // Inicie o Firebase com configuração e autenticação
  Firebase.begin(&config, &auth);

  // Reconecte-se ao Wi-Fi se necessário
  Firebase.reconnectWiFi(true);

  // Inicialize e obtenha o tempo
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

// Função para conectar ao Wi-Fi
void connectWiFi() {
  // Inicia a conexão Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  // Espera até que a conexão seja estabelecida
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  // Imprime o endereço IP local
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Função para realizar a leitura da temperatura e da turbidez
void readSensors() {
  // Realiza a leitura analógica do sensor de turbidez
  leitura = analogRead(sensorPin);
  Serial.print("Valor lido: "); // Imprime o valor lido no monitor serial
  Serial.print(leitura); 
  Serial.println(" NTU"); // Imprime a unidade NTU (Unidade Nefelométrica de Turbidez)
  delay(500); // Intervalo de 0,5 segundos entre as leituras
  Serial.print("Estado da água: "); // Imprime no monitor serial

  // Verifica a condição da água com base na leitura
  if (leitura > 700) { // Se o valor de leitura analógica estiver acima de 700
    Serial.println("LIMPA");
    condicao = "Limpa"; // Define a condição como "Limpa"
  } else if ((leitura > 600) && (leitura <= 700)) { // Se o valor de leitura analógica estiver entre 600 e 700
    Serial.println("UM POUCO SUJA"); // Define a condição como "Um Pouco Suja"
    condicao = "Um Pouco Suja";
  } else { // Se o valor de leitura analógica estiver abaixo de 600 
    Serial.println("MUITO SUJA"); // Define a condição como "Muito Suja"
    condicao = "Muito Suja";
  }

  // Solicita a temperatura do sensor DS18B20
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  // Imprime a temperatura no monitor serial
  Serial.print(temperatureC);
  Serial.println("ºC");

  Serial.println();
}

// Função para enviar os dados para o banco de dados Firebase
void sendDataToFirebase() {
  // Obtenha a hora atual
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter o tempo");
    return;
  }

  // Formata a data e a hora como strings no formato dia/mês/ano
  char dateStr[20];
  char timeStr[20];
  strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

  // Verifica se os valores são válidos (não são NaN)
  if (!isnan(leitura)) {
    // Cria um objeto FirebaseJson para armazenar os dados
    FirebaseJson content;
    content.set("fields/temperatura/stringValue", String(sensors.getTempCByIndex(0)));
    content.set("fields/turbidez/stringValue", String(leitura));
    content.set("fields/condicao/stringValue", String(condicao));
    content.set("fields/data/stringValue", String(dateStr));
    content.set("fields/horario/stringValue", String(timeStr));

    Serial.print("Foi enviado para o firebase! ");

    // Define o caminho para o documento no Firestore
    String documentPath = "leitura/TDANC70LOPQ6dZvilGoX";

    // Usa o método patchDocument para atualizar o documento no Firestore
    if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "temperatura,turbidez,condicao,data,horario")) {
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
    } else {
      Serial.println(fbdo.errorReason());
    }
  } else {
    Serial.println("Falhou");
  }
}

void loop() {
  // Lê os sensores e envia os dados para o Firebase a cada 10 segundos
  readSensors();
  sendDataToFirebase();
  delay(10000);
}