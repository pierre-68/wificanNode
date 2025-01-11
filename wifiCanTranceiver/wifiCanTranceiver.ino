#include <mcp_can.h>
#include <SPI.h>
#include <Arduino.h> // S'assurer de l'inclusion pour attachInterrupt

// MCP2515 CS et INT
#define CAN0_INT 4
#define CAN0_CS  5

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];

MCP_CAN CAN0(CAN0_CS); // CS sur pin 5

// Handles pour les tâches
TaskHandle_t xTaskInteruptMessage = NULL;

// Sémaphore pour synchroniser ISR et tâche
static SemaphoreHandle_t xSemaphore = NULL;

void IRAM_ATTR canInterruptHandler() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Donner le sémaphore pour débloquer la tâche
  xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
  if(xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void setup() {
  Serial.begin(115200);

  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
  } else {
    Serial.println("Error Initializing MCP2515...");
  }

  CAN0.setMode(MCP_NORMAL);

  // Créer un sémaphore binaire
  xSemaphore = xSemaphoreCreateBinary();
  if(xSemaphore == NULL) {
    Serial.println("Erreur lors de la création du sémaphore");
    while(1);
  }

  // Configurer le pin d'interruption
  pinMode(CAN0_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CAN0_INT), canInterruptHandler, FALLING);

  // Créer la tâche qui sera déclenchée par l'interruption
  xTaskCreate(vTaskInteruptMessage,
              "TaskInteruptMessage",
              10000,
              NULL,
              tskIDLE_PRIORITY + 1,
              &xTaskInteruptMessage);
}

void vTaskInteruptMessage(void* pvParameters) {
  for (;;) {
    // Attendre le sémaphore donné par l'interruption
    if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
      // Lire le message CAN ici
      if(CAN0.checkReceive() == CAN_MSGAVAIL) {
        CAN0.readMsgBuf(&rxId, &len, rxBuf);

        if ((rxId & 0x80000000) == 0x80000000) {
          sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
        } else {
          sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
        }
        Serial.print(msgString);

        if ((rxId & 0x40000000) == 0x40000000) {
          sprintf(msgString, " REMOTE REQUEST FRAME");
          Serial.print(msgString);
        } else {
          for (byte i = 0; i < len; i++) {
            sprintf(msgString, " 0x%.2X", rxBuf[i]);
            Serial.print(msgString);
          }
        }
        Serial.println();
      }
    }
  }
}

void loop() {
  // Boucle principale vide, tout se passe dans les tâches.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
