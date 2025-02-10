#include <GCodeParser.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <esp_now.h>

uint8_t serverAddress[] = {0x34, 0x5F, 0x45, 0xAA, 0x68, 0x64};

typedef struct struct_message {
    char message[32];
} struct_message;

struct_message sendData;
struct_message receiveData;

#define L_SERVO_PIN 5
#define R_SERVO_PIN 7

constexpr auto PIN_SENSOR = 4;
#define OPERATING_VOLTAGE 3.3

Servo lServo;
Servo rServo;

bool flag = false;

GCodeParser GCode = GCodeParser();

/*#################################################################################################
#                                        ОБРАБОТКА ESP_NOW                                        #
#################################################################################################*/

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print(";(Статус отправки: ");
    Serial.print(status == ESP_NOW_SEND_SUCCESS ? "Успех" : "Ошибка");
    Serial.println(")");
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    memcpy(&receiveData, incomingData, sizeof(receiveData));
    Serial.print(";(Ответ от сервера: ");
    Serial.print(receiveData.message);
    Serial.println(")");

    if (strcmp(receiveData.message, "M3") == 0) {
      Serial.println(";(Команда: goCar)");
      goCar();
    } else if (strcmp(receiveData.message, "M4") == 0) {
      Serial.println(";(Команда: backCar)");
      backCar();
    } else if (strcmp(receiveData.message, "M2") == 0) {
      Serial.println(";(Команда: stopCar)");
      stopCar();
    }
}

void ESP_NOWInit() {
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
      Serial.println(";(Ошибка инициализации ESP-NOW)");
      return;
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, serverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println(";(Ошибка добавления сервера)");
      return;
  }
}

void sendMess(String msg) {
  strncpy(sendData.message, msg.c_str(), sizeof(sendData.message));
  esp_err_t result = esp_now_send(serverAddress, (uint8_t *)&sendData, sizeof(sendData));

  Serial.println(result == ESP_OK ? ";(Сообщение отправлено!)" : ";(Ошибка отправки!)");
}
/*#################################################################################################
#                                        УПРАВЛЕНИЕ СЕРВО                                         #
#################################################################################################*/

void goCar() {
  while (true) {
    int valueSensor = analogRead(PIN_SENSOR);
    float voltageSensor = valueSensor * OPERATING_VOLTAGE / 4095.0;
    lServo.write(112);
    rServo.write(70);

    if (voltageSensor < 1.0) {
      flag = true;
    } else if ((voltageSensor > 1.0) and (flag)) {
      lServo.write(90);
      rServo.write(90);
      flag = false;
      Serial.println("finish");
      sendMess("M2");
      return;
    }
  }
}

void stopCar() {
  Serial.println(";(stop car)");
  lServo.write(90);
  rServo.write(90);
}

void backCar() {
  while (true) {
    int valueSensor = analogRead(PIN_SENSOR);
    float voltageSensor = valueSensor * OPERATING_VOLTAGE / 4095.0;
    lServo.write(70);
    rServo.write(112);

    if (voltageSensor < 1.0) {
      flag = true;
    } else if ((voltageSensor > 1.0) and (flag)) {
      lServo.write(90);
      rServo.write(90);
      flag = false;
      Serial.println("finish");
      sendMess("M2");
      return;
    }
  }
}

/*#################################################################################################
#                                           SETUP                                                #
#################################################################################################*/

void setup() {
  Serial.begin(115200);
  delay(1000);

  ESP_NOWInit();
  delay(100);
  lServo.attach(L_SERVO_PIN, 500, 2500);
  delay(100);
  rServo.attach(R_SERVO_PIN, 500, 2500);
}

/*#################################################################################################
#                                           LOOP                                                 #
#################################################################################################*/

void loop() {
}
