#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>
#include <EEPROM.h>

// Pinos
#define DHTPIN        2
#define DHTTYPE       DHT11
#define LDR_PIN       A0
#define LED_VERDE     8
#define LED_AMARELO   9
#define LED_VERMELHO  10
#define BUZZER        7
#define BTN1          4
#define BTN2          5

// Limites ideais
#define TEMP_MAX    16.0
#define UMID_MIN    60.0
#define UMID_MAX    80.0
#define LUZ_MAX     60

// EEPROM
#define EEPROM_START  0
#define EEPROM_SIZE   512
#define RECORD_SIZE   8
int eepromAddr = EEPROM_START;

// Objetos
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

// Configuracoes do usuario
int  utcOffset  = -3;   // Brasilia = -3 | Londres = 0 ou +1
bool useCelsius = true;
int  idioma     = 0;    // 0=PT  1=EN

// Acumuladores para media de 10s
float somaTemp = 0, somaUmid = 0;
int   somaLuz  = 0, contLeituras = 0;
unsigned long tInicio = 0;

// CARACTERES CUSTOMIZADOS
byte charGrau[8] = {0x06,0x09,0x09,0x06,0x00,0x00,0x00,0x00};
byte charGota[8] = {0x04,0x04,0x0E,0x1F,0x1F,0x1F,0x0E,0x00};
byte charSol[8]  = {0x00,0x0A,0x04,0x1B,0x04,0x0A,0x00,0x00};
byte charOK[8]   = {0x00,0x01,0x03,0x16,0x1C,0x08,0x00,0x00};
byte charBell[8] = {0x04,0x0E,0x0E,0x0E,0x1F,0x00,0x04,0x00};

byte charBar[8]  = {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F};

byte c1_11[]  = { B00010,B00010,B00010,B00010,B00001,B00000,B00000,B00000 };
byte f0_12[]  = { B00000,B00000,B00000,B00001,B00011,B00010,B00010,B00010 };
byte c0_13[]  = { B00000,B00000,B00000,B11111,B11111,B00000,B00000,B00000 };
byte c1_14[]  = { B00000,B01111,B11111,B11111,B11111,B11111,B01111,B00000 };
byte c0_15[]  = { B00000,B11111,B11111,B11111,B11111,B11111,B11111,B00000 };
byte c1_13[]  = { B01000,B01000,B01000,B01000,B10000,B00000,B00000,B00000 };

// Frames animados do liquido na taca
byte f1_12_frame1[] = { B00110,B00100,B00100,B00100,B10101,B10101,B01110,B00100 };
byte f1_12_frame2[] = { B00110,B00100,B00100,B00100,B11111,B11111,B01110,B00100 };
byte f1_12_frame3[] = { B00110,B00100,B11111,B11111,B11111,B11111,B01110,B00100 };

// DECLARACAO DE FUNCOES
void animacaoTaca();
void rodarCicloTaca();
void desenharTacaCompleta();
void animacaoLogo();
void menuConfiguracaoInicial();
void imprimirUTC();
void feedbackConfirm();
void telaInicio();
void exibirLCD(float t, float h, int luz);
void acionarAlertas(float t, float h, int luz);
void mostrarAlerta(const char* msg, float val, char tipo);
void gravarEEPROM(float t, float h, int luz);
void logSerial(float t, float h, int luz);
int  horaLocal();
void esperarSoltar(int pino);
bool lerBotao();

void esperarSoltar(int pino) {
  while (digitalRead(pino) == LOW);
  delay(150);
}

bool lerBotao() {
  while (true) {
    if (digitalRead(BTN1) == LOW) { esperarSoltar(BTN1); return true;  }
    if (digitalRead(BTN2) == LOW) { esperarSoltar(BTN2); return false; }
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(LED_VERDE,    OUTPUT);
  pinMode(LED_AMARELO,  OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER,       OUTPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);

  Wire.begin();

  // Detecta endereco I2C do LCD (0x27 ou 0x3F)
  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() != 0) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Animacao da taca enchendo
  animacaoTaca();

  // "Inicializando" + barra
  lcd.createChar(6, charBar);
  animacaoLogo();

  // Carrega definitivamente os chars do loop
  lcd.createChar(0, charGrau);
  lcd.createChar(1, charGota);
  lcd.createChar(2, charSol);
  lcd.createChar(3, charOK);
  lcd.createChar(4, charBell);

  dht.begin();

  if (!rtc.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("Erro: RTC!");
    while (1);
  }
  // RTC sempre armazena hora UTC.
  // O offset do usuario e aplicado SOMENTE na exibicao e na EEPROM.
  // Assim Londres (UTC+0) e Brasilia (UTC-3) ficam corretos
  // desde que o RTC esteja ajustado com a hora UTC real.
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Menu de configuracao inicial
  menuConfiguracaoInicial();
  telaInicio();

  tInicio = millis();
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int ldrBruto = analogRead(LDR_PIN);
  int luz = map(ldrBruto, 0, 1023, 0, 100);

  if (!isnan(t) && !isnan(h)) {
    somaTemp += t;
    somaUmid += h;
    somaLuz  += luz;
    contLeituras++;
    exibirLCD(t, h, luz);
  }

  if (millis() - tInicio >= 10000 && contLeituras > 0) {
    float mT = somaTemp / contLeituras;
    float mH = somaUmid / contLeituras;
    int   mL = somaLuz  / contLeituras;

    acionarAlertas(mT, mH, mL);
    gravarEEPROM(mT, mH, mL);
    logSerial(mT, mH, mL);

    somaTemp = somaUmid = somaLuz = contLeituras = 0;
    tInicio = millis();
  }

  delay(1000);
}

// O RTC guarda UTC. O offset e somado apenas aqui,
// garantindo que qualquer fuso escolhido no menu
// (ex: UTC+0 para Londres, UTC-3 para Brasilia)
// exiba o horario correto sem alterar o RTC.
int horaLocal() {
  DateTime now = rtc.now();
  return ((int)now.hour() + utcOffset + 24) % 24;
}

void rodarCicloTaca() {
  lcd.createChar(6, f1_12_frame1);
  desenharTacaCompleta();
  delay(700);

  lcd.createChar(6, f1_12_frame2);
  desenharTacaCompleta();
  delay(700);

  lcd.createChar(6, f1_12_frame3);
  desenharTacaCompleta();
  delay(900);
}

void animacaoTaca() {
  lcd.createChar(0, c1_11);
  lcd.createChar(1, f0_12);
  lcd.createChar(2, c0_13);
  lcd.createChar(3, c1_14);
  lcd.createChar(4, c0_15);
  lcd.createChar(5, c1_13);

  // Ciclo 1
  rodarCicloTaca();
  // Ciclo 2
  rodarCicloTaca();
}

void desenharTacaCompleta() {
  lcd.clear();
  lcd.setCursor(3, 0); lcd.print("InVino");
  lcd.setCursor(0, 1); lcd.print("Boas-vindas");

  // Garrafa (linha superior)
  lcd.setCursor(12, 0); lcd.write(byte(1));
  lcd.setCursor(13, 0); lcd.write(byte(2));
  lcd.setCursor(14, 0); lcd.write(byte(3));
  lcd.setCursor(15, 0); lcd.write(byte(4));

  // Taca (linha inferior)
  lcd.setCursor(11, 1); lcd.write(byte(0));
  lcd.setCursor(12, 1); lcd.write(byte(6));
  lcd.setCursor(13, 1); lcd.write(byte(5));
}

void animacaoLogo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Inicializando  "); 

  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    lcd.write(byte(6));
    delay(90);
  }
  delay(500);

  for (int i = 0; i < 3; i++) {
    lcd.noBacklight(); delay(160);
    lcd.backlight();   delay(160);
  }
  delay(300);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Monitoramento ");
  lcd.setCursor(0, 1);
  lcd.print("  de Ambiente   ");
  delay(1400);
  lcd.clear();
}

void menuConfiguracaoInicial() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("> CONFIGURACAO  ");
  lcd.setCursor(0, 1);
  lcd.print("B1=OK  B2=Alter.");
  delay(2000);

  // Idioma
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1/3 Language:   ");
  lcd.setCursor(0, 1);
  lcd.print(idioma == 0 ? "> Portugues     " : "> English       ");

  while (true) {
    bool b = lerBotao();
    if (b) break;
    idioma = (idioma == 0) ? 1 : 0;
    lcd.setCursor(0, 1);
    lcd.print(idioma == 0 ? "> Portugues     " : "> English       ");
  }
  feedbackConfirm();

  // Unidade de temperatura
  lcd.clear();
  lcd.setCursor(0, 0);
  // Titulo respeita idioma escolhido
  lcd.print(idioma == 0 ? "2/3 Temperatura:" : "2/3 Temperature:");
  lcd.setCursor(0, 1);
  lcd.print(useCelsius ? "> Celsius (C)   " : "> Fahrenheit(F) ");

  while (true) {
    bool b = lerBotao();
    if (b) break;
    useCelsius = !useCelsius;
    lcd.setCursor(0, 1);
    lcd.print(useCelsius ? "> Celsius (C)   " : "> Fahrenheit(F) ");
  }
  feedbackConfirm();

  // Fuso horario
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(idioma == 0 ? "3/3 Fuso (UTC): " : "3/3 Timezone:   ");
  lcd.setCursor(0, 1);
  imprimirUTC();

  while (true) {
    bool b = lerBotao();
    if (b) break;
    utcOffset++;
    if (utcOffset > 14) utcOffset = -12;
    lcd.setCursor(0, 1);
    imprimirUTC();
  }
  feedbackConfirm();
}

void imprimirUTC() {
  lcd.print("> UTC");
  if (utcOffset >= 0) lcd.print("+");
  lcd.print(utcOffset);
  lcd.print("          ");
}

void feedbackConfirm() {
  tone(BUZZER, 1000); delay(80); noTone(BUZZER);
  lcd.setCursor(15, 1);
  lcd.write(byte(3));
  delay(400);
}

// TELA DE INICIO
void telaInicio() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(idioma == 0 ? "Config. salva!" : "Settings saved!");
  lcd.setCursor(0, 1);
  lcd.print(useCelsius ? "C " : "F ");
  lcd.print("UTC");
  if (utcOffset >= 0) lcd.print("+");
  lcd.print(utcOffset);
  lcd.print(idioma == 0 ? " PT" : " EN");
  delay(1800);

  for (int i = 3; i >= 1; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(idioma == 0 ? "Iniciando em..." : "Starting in...");
    lcd.setCursor(7, 1);
    lcd.print(i);
    tone(BUZZER, 800 + i * 100); delay(200); noTone(BUZZER);
    delay(800);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(idioma == 0 ? "   Monitorando!" : "  Monitoring!  ");
  tone(BUZZER, 1200); delay(300); noTone(BUZZER);
  delay(800);
  lcd.clear();
}

// EXIBIR LEITURA NO LCD
void exibirLCD(float t, float h, int luz) {
  DateTime now = rtc.now();
  int hora = horaLocal();

  float tShow = useCelsius ? t : (t * 9.0 / 5.0 + 32.0);
  char  uni   = useCelsius ? 'C' : 'F';

  lcd.setCursor(0, 0);
  lcd.print("T:");
  if (tShow < 10) lcd.print(" ");
  lcd.print(tShow, 1);
  lcd.write(byte(0));
  lcd.print(uni);
  lcd.print(" ");
  lcd.write(byte(1));
  lcd.print(":");
  if ((int)h < 10) lcd.print(" ");
  lcd.print((int)h);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.write(byte(2));
  lcd.print(":");
  if (luz < 10) lcd.print(" ");
  lcd.print(luz);
  lcd.print("% ");
  if (hora < 10)          lcd.print("0");
  lcd.print(hora);        lcd.print(":");
  if (now.minute() < 10)  lcd.print("0");
  lcd.print(now.minute()); lcd.print(":");
  if (now.second() < 10)  lcd.print("0");
  lcd.print(now.second());
}

// ALERTAS 
void acionarAlertas(float t, float h, int luz) {
  bool aT = (t > TEMP_MAX);
  bool aU = (h < UMID_MIN) || (h > UMID_MAX);
  bool aL = (luz > LUZ_MAX);

  digitalWrite(LED_VERDE,    LOW);
  digitalWrite(LED_AMARELO,  LOW);
  digitalWrite(LED_VERMELHO, LOW);
  noTone(BUZZER);

  if (aT) {
    digitalWrite(LED_VERMELHO, HIGH);
    for (int i = 0; i < 3; i++) {
      tone(BUZZER, 1200); delay(200);
      noTone(BUZZER);     delay(100);
    }
    mostrarAlerta(idioma == 0 ? "ALERTA TEMP!" : "TEMP ALERT!", t, 'T');
  } else if (aU) {
    digitalWrite(LED_AMARELO, HIGH);
    tone(BUZZER, 800); delay(400); noTone(BUZZER);
    mostrarAlerta(idioma == 0 ? "ALERTA UMID!" : "HUMID ALERT!", h, 'U');
  } else if (aL) {
    digitalWrite(LED_AMARELO, HIGH);
    tone(BUZZER, 600); delay(300); noTone(BUZZER);
    mostrarAlerta(idioma == 0 ? "ALERTA LUZ!" : "LIGHT ALERT!", luz, 'L');
  } else {
    digitalWrite(LED_VERDE, HIGH);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(3));
    lcd.print(idioma == 0 ? " Tudo normal!" : " All good!");
    lcd.setCursor(0, 1);
    lcd.print(idioma == 0 ? "Media 10s: OK  " : "10s avg:  OK   ");
    delay(1500);
    lcd.clear();
  }
}

void mostrarAlerta(const char* msg, float val, char tipo) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(byte(4));
  lcd.print(" ");
  lcd.print(msg);
  lcd.setCursor(0, 1);
  if (tipo == 'T') {
    lcd.print(idioma == 0 ? "Med:" : "Avg:");
    lcd.print(val, 1);
    lcd.write(byte(0));
    lcd.print(useCelsius ? "C" : "F");
  } else if (tipo == 'U') {
    lcd.print(idioma == 0 ? "Med Umid:" : "Avg Hum: ");
    lcd.print((int)val); lcd.print("%");
  } else {
    lcd.print(idioma == 0 ? "Med Luz: " : "Avg Lux: ");
    lcd.print((int)val); lcd.print("%");
  }
  delay(2500);
  lcd.clear();
}

// EEPROM – GRAVAR
void gravarEEPROM(float t, float h, int luz) {
  DateTime now = rtc.now();
  int hora = horaLocal();

  if (eepromAddr + RECORD_SIZE >= EEPROM_SIZE)
    eepromAddr = EEPROM_START;

  EEPROM.write(eepromAddr,     (int)t);
  EEPROM.write(eepromAddr + 1, (int)((t - (int)t) * 10));
  EEPROM.write(eepromAddr + 2, (int)h);
  EEPROM.write(eepromAddr + 3, (int)((h - (int)h) * 10));
  EEPROM.write(eepromAddr + 4, luz);
  EEPROM.write(eepromAddr + 5, hora);
  EEPROM.write(eepromAddr + 6, now.minute());
  EEPROM.write(eepromAddr + 7, now.second());

  eepromAddr += RECORD_SIZE;
}

void logSerial(float t, float h, int luz) {
  DateTime now = rtc.now();
  int hora = horaLocal();
  bool pt = (idioma == 0);

  Serial.print(pt ? "Hora (UTC" : "Time (UTC");
  Serial.print(utcOffset >= 0 ? "+" : "");
  Serial.print(utcOffset); Serial.print("): ");
  if (hora < 10)         Serial.print("0"); Serial.print(hora);          Serial.print(":");
  if (now.minute() < 10) Serial.print("0"); Serial.print(now.minute());  Serial.print(":");
  if (now.second() < 10) Serial.print("0"); Serial.println(now.second());

  Serial.print(pt ? "Temperatura : " : "Temperature : "); Serial.print(t, 1);
  Serial.println(useCelsius ? " C" : " F");
  Serial.print(pt ? "Umidade     : " : "Humidity    : "); Serial.print(h, 1); Serial.println(" %");
  Serial.print(pt ? "Luminosidade: " : "Light level : "); Serial.print(luz);  Serial.println(" %");

  bool aT = t > TEMP_MAX;
  bool aU = h < UMID_MIN || h > UMID_MAX;
  bool aL = luz > LUZ_MAX;
  Serial.print("Status: ");
  if (!aT && !aU && !aL) Serial.println(pt ? "OK - Tudo normal" : "OK - All good");
  if (aT) Serial.println(pt ? "ALERTA - Temperatura!" : "ALERT - Temperature!");
  if (aU) Serial.println(pt ? "ALERTA - Umidade!"     : "ALERT - Humidity!");
  if (aL) Serial.println(pt ? "ALERTA - Luminosidade!": "ALERT - Light level!");
  Serial.println();
}
