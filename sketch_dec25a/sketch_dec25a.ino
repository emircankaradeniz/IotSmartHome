#include <SoftwareSerial.h> // SoftwareSerial kütüphanemizi ekliyoruz.
#include <DHT.h>  // DHT11 sensör kütüphanemizi ekliyoruz.
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// --- Pin Tanımları ---
#define LDR_PIN A0
#define PIR_PIN 2
#define MQ2_PIN A0 // A1 yerine A0 kullanılacak
#define DHT_PIN 3
#define DHT_TYPE DHT11
#define SERVO_PIN 5
#define RST_PIN 9
#define SDA_PIN 10
#define LED_MOTION 8

#define ESP_RX 6  // Arduino RX (ESP8266 TX)
#define ESP_TX 7  // Arduino TX (ESP8266 RX)

DHT dht(DHT_PIN, DHT_TYPE); // DHT sensörü tanımlama

// Wi-Fi bilgileri
String agAdi = "POCO X5 Pro 5G"; // Ağ adı
String agSifresi = "22222222"; // Ağ şifresi

// ThingSpeak API bilgileri
String ip = "184.106.153.149"; // ThingSpeak IP adresi
String apiKey = "73E22XA19IDQ6C8O"; // ThingSpeak API anahtarı

// Sensör eşikleri
int mq2Threshold = 300;
byte authorizedUID[] = {0xDE, 0xAD, 0xBE, 0xEF}; // Yetkili kart UID

// Nesne tanımları
SoftwareSerial esp(ESP_RX, ESP_TX);
MFRC522 rfid(SDA_PIN, RST_PIN);
Servo myServo;

// Veriler
double sicaklik, nem;
int pirValue, ldrValue, mq2Value;
bool authorized = false;

void setup() {
  Serial.begin(9600);
  Serial.println("Başlatılıyor...");
  esp.begin(115200);

  // Wi-Fi bağlantısı
  esp.println("AT");
  while (!esp.find("OK")) {
    esp.println("AT");
    Serial.println("ESP8266 Bulunamadı.");
  }
  Serial.println("ESP Bağlantısı Kuruldu.");

  esp.println("AT+CWMODE=1"); // ESP8266 modülünü client moduna ayarla
  while (!esp.find("OK")) {
    esp.println("AT+CWMODE=1");
  }

  esp.println("AT+CWJAP=\"" + agAdi + "\",\"" + agSifresi + "\"");
  while (!esp.find("OK")) {
    Serial.println("Ağa Bağlanılıyor...");
  }
  Serial.println("Wi-Fi Bağlantısı Tamamlandı!");

  // Donanım başlatma
  SPI.begin();
  rfid.PCD_Init();
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  dht.begin(); // DHT sensörü başlatma
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(MQ2_PIN, INPUT);

  Serial.println("Kurulum Tamamlandı!");
}

void loop() {

  // if ( ! rfid.PICC_ReadCardSerial()){ 
  //   ekranaYazdir();  
  //   delay(3000); //Yeni kartın okunmasını bekliyoruz.
  //   return;
  // }
  int pirVal   = digitalRead(PIR_PIN);
  if (pirVal == HIGH) {
    digitalWrite(LED_MOTION, HIGH);
  } else {
    digitalWrite(LED_MOTION, LOW);
  }
  // Sensör verilerini oku
  sicaklik = dht.readTemperature();
  nem = dht.readHumidity();
  pirValue = digitalRead(PIR_PIN);
  ldrValue = analogRead(LDR_PIN);
  mq2Value = analogRead(MQ2_PIN);

  // RFID kontrolü
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Kart UID'sini seri monitöre yazdır
    Serial.print("Kart UID: ");
    for (byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(rfid.uid.uidByte[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    
    // Kartı yetkili UID ile karşılaştır
    if (checkUID(rfid.uid.uidByte, rfid.uid.size)) {
      authorized = true;
      Serial.println("Yetkili kart algılandı! Kapı açılıyor...");
      openDoor();
    } else {
      authorized = false;
      Serial.println("Yetkisiz kart algılandı!");
      ekranaYazdir();
    }
    rfid.PICC_HaltA();
  }

  // ThingSpeak'e verileri gönder
  sendToThingSpeak();

  delay(10000); // ThingSpeak minimum bekleme süresi
}

void sendToThingSpeak() {
  esp.println("AT+CIPSTART=\"TCP\",\"" + ip + "\",80"); // TCP bağlantısı başlat
  if (esp.find("Error")) {
    Serial.println("Bağlantı Hatası!");
    return;
  }

  String veri = "GET https://api.thingspeak.com/update?api_key=" + apiKey;
  veri += "&field1=" + String(sicaklik);
  veri += "&field2=" + String(nem);
  veri += "&field3=" + String(mq2Value);
  veri += "&field4=" + String(ldrValue);
  veri += "&field5=" + String(pirValue);
  veri += "&field6=" + String(authorized ? 1 : 0);
  veri += "\r\n\r\n";

  esp.print("AT+CIPSEND=");
  esp.println(veri.length());
  delay(2000);

  if (esp.find(">")) {
    esp.print(veri);
    Serial.println("Veri Gönderiliyor: " + veri);
  } else {
    Serial.println("Veri gönderilemedi.");
  }

  esp.println("AT+CIPCLOSE"); // Bağlantıyı kapat
  Serial.println("Bağlantı Kapandı.");
}

bool checkUID(byte *uid, byte uidSize) {
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] != authorizedUID[i]) return false;
  }
  return true;
}

// Kapıyı açan fonksiyon
void openDoor() {
  myServo.write(90); // Servo motoru 90 dereceye getir (kapıyı aç)
  delay(3000);       // Kapı 3 saniye açık kalsın
  myServo.write(0);  // Servo motoru başlangıç pozisyonuna getir (kapıyı kapat)
}
void ekranaYazdir(){
  Serial.print("ID Numarasi: ");
  for(int sayac = 0; sayac < 4; sayac++){
    Serial.print(rfid.uid.uidByte[sayac]);
    Serial.print(" ");
  }
  Serial.println("");
}
