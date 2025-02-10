#include <WiFi.h>
#include <esp_now.h>
#include <GCodeParser.h>

#define HOME_BTN 4
#define INIT_BTN 2
#define STEP_PIN 18
#define DIR_PIN 19
#define ENB_PIN 21

int floorStep = 2635;
int curentFloor = 1;

bool flag = false;
uint32_t btnTimer = 0;

typedef struct struct_message {
  char message[32];
} struct_message;

struct_message receiveData;
struct_message sendData;

// Указываем MAC-адрес клиента вручную
uint8_t clientAddress[] = {0x64, 0xE8, 0x33, 0x8D, 0x6F, 0x4C};

GCodeParser GCode = GCodeParser();

/*####################################################################################################################################################
----------------------------------------------Функции инициализации переферийных устройств------------------------------------------------------------
####################################################################################################################################################*/

void homeBtnInit() {
  pinMode(HOME_BTN, INPUT_PULLUP);
}

bool homeBtnCheck() {
  return digitalRead(HOME_BTN);
}

void initBtnInit() {
  pinMode(INIT_BTN, INPUT_PULLUP);
}

void liftInit() {
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
}

void liftStop() {
  digitalWrite(ENB_PIN, HIGH);
}

void liftEnb() {
  digitalWrite(ENB_PIN, LOW);
}

/*############################################################################################################################################# 
-----------------------------------------------------отправка G-кодов обратно на rpi-----------------------------------------------------------
#############################################################################################################################################*/

void finishTask() {
  Serial.println("S2");
}

void initToPy() {
  Serial.println("S8");
}

/*############################################################################################################################################# 
------------------------------------------------функции связаные с работой ESP-NOW сервера-----------------------------------------------------
#############################################################################################################################################*/

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print(";(Статус отправки: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Успех)" : "Ошибка)");
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  memcpy(&receiveData, incomingData, sizeof(receiveData));

  Serial.print(";(Получено от клиента: ");
  Serial.print(receiveData.message);
  Serial.println(")");

  if (strcmp(receiveData.message, "M2") == 0) {
    Serial.println("S2");
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
  memcpy(peerInfo.peer_addr, clientAddress, 6);
  peerInfo.channel = 0;                        
  peerInfo.encrypt = false;                    

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println(";(Ошибка добавления клиента)");
    return;
  }
}

void sendMess(String msg) {
  strncpy(sendData.message, msg.c_str(), sizeof(sendData.message));
  esp_err_t result = esp_now_send(clientAddress, (uint8_t *)&sendData, sizeof(sendData));
}

/*###########################################################################################################################################
--------------------------------------------------------------отработка G-кодов--------------------------------------------------------------
###########################################################################################################################################*/

void liftHome() {
  Serial.println(";(движение в начальное положение)");
  liftEnb();
  digitalWrite(DIR_PIN, HIGH);
  while (digitalRead(HOME_BTN)) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2000);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(2000);
  }
  for (int i = 0; i < 220; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2000);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(2000);
  }
  liftStop();
  curentFloor = 1;

  Serial.println(";(начальное положение)");
  finishTask();
}

void liftToFloor(int floor) {
  Serial.print(";(Движение к этажу");
  Serial.print(floor);
  Serial.println(")");
  liftEnb();
  while (curentFloor != floor) {
    if (curentFloor > floor) {
      digitalWrite(DIR_PIN, HIGH);
      for (int i = 0; i < floorStep; i++) {
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(1000);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(1000);
      }
      curentFloor -= 1;
      Serial.print(";(Текущий этаж:");
      Serial.print(curentFloor);
      Serial.println(")");
    }

    if (curentFloor < floor) {
      digitalWrite(DIR_PIN, LOW);
      for (int i = 0; i < floorStep; i++) {
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(1000);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(1000);
      }
      curentFloor += 1;
      Serial.print(";(Текущий этаж:");
      Serial.print(curentFloor);
      Serial.println(")");
    }
  }
  finishTask();
}

/*###########################################################################################################################################
--------------------------------------------------------------setup() и loop()---------------------------------------------------------------
###########################################################################################################################################*/

void setup() {
  Serial.begin(9600);
  delay(500);
  ESP_NOWInit();
  delay(1000);
  homeBtnInit();
  initBtnInit();
  liftInit();
  delay(500);
  //liftHome();
}


void loop() {
  {
    if (!digitalRead(INIT_BTN)) {
      if (Serial.available() > 0) {
        if (GCode.AddCharToLine(Serial.read())) {
          GCode.ParseLine();
          GCode.RemoveCommentSeparators();
          if (GCode.HasWord('G')) {
            int code = (int)GCode.GetWordValue('G');
            /*Serial.print(";(");
            Serial.print(code);
            Serial.println(")");*/
            if (code == 3) {
              sendMess("M3");
            } else if (code == 4) {
              sendMess("M4");
            } else if (code == 2) {
              sendMess("M2");
            } else if (code == 28) {
              liftHome();
            } else if (code == 1) {
              int floor = (GCode.HasWord('H')) ? (int)GCode.GetWordValue('H') : -1;
              liftToFloor(floor);
              //Serial.println(floor);
            }
          }
        }
      }
    } else {
      bool btnState = !digitalRead(2);
      if (btnState && !flag && millis() - btnTimer > 10) {
        flag = true;
        btnTimer = millis();
        Serial.println("S8");
      }
      if (!btnState && flag && millis() - btnTimer > 10) {
        flag = false;
        btnTimer = millis();
        //Serial.println("release");
      }
    }
  }
}
