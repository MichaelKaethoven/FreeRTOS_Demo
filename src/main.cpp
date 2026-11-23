/*
 * FreeRTOS-demo - ESP32 (DFRobot FireBeetle 2 ESP32-E), Arduino-framework
 * ============================================================================
 *
 * WAT IS DIT?
 *   Een klein voorbeeld van FreeRTOS-taken op de ESP32. Een FreeRTOS-taak is
 *   zoals een thread: ze heeft een eigen stack, en de FreeRTOS-scheduler
 *   beslist wanneer ze draait. Meerdere taken lijken tegelijk te draaien
 *   (time-slicing). Omdat de ESP32 twee cores heeft, kunnen twee taken zelfs
 *   echt parallel draaien.
 *
 *   DEEL 1 is de minimale werkende code: twee onafhankelijke taken.
 *   DEEL 2 bevat optionele uitbreidingen (queue, mutex, monitor). Zet ze aan
 *   met de schakelaars hieronder. De minimale versie werkt zonder.
 *
 * HARDWARE
 *   - Ingebouwde LED op GPIO 2
 *   - Drukknop tussen GPIO 27 en GND (alleen voor ADD_QUEUE)
 *
 * vTaskDelay() vs delay()
 *   vTaskDelay() laat ALLEEN de taak die hem aanroept slapen en geeft de CPU
 *   aan de andere taken. Er wordt verder niets geblokkeerd, dus dit is nog
 *   steeds niet-blokkerende code. (Op de ESP32 roept Arduino's delay() zelf
 *   vTaskDelay() aan. Dit voorbeeld gebruikt vTaskDelay() om dat expliciet
 *   te maken.)
 *
 * BRONNEN
 *   FreeRTOS op de ESP32 (ESP-IDF):
 *     https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos.html
 *   FreeRTOS API-referentie (taken, queues, semaforen):
 *     https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/00-TaskHandle
 *   Meer uitleg: zie README.md in deze map.
 */

#include <Arduino.h>

// ---- Pinnen ----
#define LED_PIN 2
#define BUTTON_PIN 27

// ---- Optionele uitbreidingen (DEEL 2): 1 = aan, 0 = uit ----
#define ADD_QUEUE 0   // Knoptaak stuurt drukken door naar een verwerkingstaak
#define ADD_MUTEX 0   // Beschermt Serial zodat meerdere regels samen blijven
#define ADD_MONITOR 0 // Drukt de vrije stack per taak en de vrije heap af

// ============================================================================
// DEEL 1 - MINIMALE WERKENDE CODE
// ============================================================================

// Taak 1: laat de LED elke 500 ms knipperen.
// Een taak is een gewone functie met een eindeloze lus. Ze mag NOOIT
// returnen. "param" is de waarde die je als 4e argument aan
// xTaskCreatePinnedToCore() meegeeft (hier niet gebruikt).
void blinkTask(void *param) {
  pinMode(LED_PIN, OUTPUT);
  bool ledOn = false;

  for (;;) {
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn);
    vTaskDelay(pdMS_TO_TICKS(200)); // 500 ms slapen, andere taken draaien door
  }
}

// Taak 2: drukt elke seconde een teller af, in een ander ritme dan taak 1.
// Beide taken draaien onafhankelijk: geen van beide wacht op de andere.
void counterTask(void *param) {
  uint32_t count = 0;

  for (;;) {
    Serial.printf("[counter] %lu (running on core %d)\n", count++,
                  xPortGetCoreID());
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ============================================================================
// DEEL 2 - OPTIONELE UITBREIDINGEN
// ============================================================================

#if ADD_MUTEX
// Een mutex is een "sleutel": alleen de taak die hem heeft, mag Serial
// gebruiken. Andere taken die hem willen, wachten (zonder CPU te gebruiken)
// tot hij teruggegeven wordt.
SemaphoreHandle_t serialMutex;
#define SERIAL_LOCK() xSemaphoreTake(serialMutex, portMAX_DELAY)
#define SERIAL_UNLOCK() xSemaphoreGive(serialMutex)
#else
#define SERIAL_LOCK()
#define SERIAL_UNLOCK()
#endif

#if ADD_QUEUE
// Een queue (wachtrij) geeft veilig data door van de ene taak naar de
// andere. Hier: het nummer van de knopdruk. Een gedeelde globale variabele
// kan misgaan als twee taken er op hetzelfde moment aan zitten.
QueueHandle_t buttonQueue;

// Leest de knop elke 20 ms uit. Door zo traag te pollen, sla je ook het
// "denderen" (bouncen) van de knop over.
void buttonTask(void *param) {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  bool lastState = HIGH;
  uint32_t presses = 0;

  for (;;) {
    bool state = digitalRead(BUTTON_PIN);
    if (state != lastState) {
      lastState = state;
      if (state == LOW) { // Ingedrukt (niet losgelaten)
        presses++;
        xQueueSend(buttonQueue, &presses, 0); // 0 = niet wachten als ze vol is
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Wacht op een bericht in de queue. portMAX_DELAY = eindeloos wachten.
// Tijdens het wachten gebruikt deze taak GEEN CPU-tijd: ze is "geblokkeerd"
// tot er data binnenkomt.
void buttonHandlerTask(void *param) {
  uint32_t presses;

  for (;;) {
    if (xQueueReceive(buttonQueue, &presses, portMAX_DELAY) == pdTRUE) {
      SERIAL_LOCK();
      Serial.printf("[button] pressed %lu time(s)\n", presses);
      SERIAL_UNLOCK();
    }
  }
}
#endif

#if ADD_MONITOR
// Task handles, zodat de monitor informatie over de andere taken kan opvragen
TaskHandle_t blinkHandle = nullptr;
TaskHandle_t counterHandle = nullptr;

// Elke 5 s: hoeveel stack elke taak nog nooit gebruikt heeft (de "high water
// mark"). Dicht bij 0 = de stack is te klein, verhoog hem dan in setup().
void monitorTask(void *param) {
  for (;;) {
    SERIAL_LOCK(); // Deze regels samen houden als één blok
    Serial.println("----- monitor -----");
    Serial.printf("blink   free stack: %u bytes\n",
                  uxTaskGetStackHighWaterMark(blinkHandle));
    Serial.printf("counter free stack: %u bytes\n",
                  uxTaskGetStackHighWaterMark(counterHandle));
    Serial.printf("free heap         : %u bytes\n", ESP.getFreeHeap());
    Serial.println("-------------------");
    SERIAL_UNLOCK();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
#define BLINK_HANDLE &blinkHandle
#define COUNTER_HANDLE &counterHandle
#else
#define BLINK_HANDLE nullptr
#define COUNTER_HANDLE nullptr
#endif

// ============================================================================
// SETUP & LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("FreeRTOS demo starting");

#if ADD_MUTEX
  serialMutex = xSemaphoreCreateMutex();
#endif
#if ADD_QUEUE
  buttonQueue = xQueueCreate(5, sizeof(uint32_t)); // Maximaal 5 berichten
#endif

  // xTaskCreatePinnedToCore(functie, naam, stackgrootte in bytes, parameter,
  //                         prioriteit, handle, core)
  // - Stack: 2048 is genoeg voor eenvoudig werk, Serial.printf heeft meer
  //   nodig (4096)
  // - Prioriteit: hoger getal = belangrijker. 1 = hetzelfde als loop()
  // - Core 1: core 0 wordt vooral door Wi-Fi/Bluetooth gebruikt
  xTaskCreatePinnedToCore(blinkTask, "blink", 2048, nullptr, 1, BLINK_HANDLE,
                          1);
  xTaskCreatePinnedToCore(counterTask, "counter", 4096, nullptr, 1,
                          COUNTER_HANDLE, 1);

#if ADD_QUEUE
  xTaskCreatePinnedToCore(buttonTask, "button", 2048, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(buttonHandlerTask, "btnHandler", 4096, nullptr, 1,
                          nullptr, 1);
#endif
#if ADD_MONITOR
  // Bewust op core 0: toont dat taken op beide cores kunnen draaien
  xTaskCreatePinnedToCore(monitorTask, "monitor", 4096, nullptr, 1, nullptr, 0);
#endif
}

void loop() {
  // Op de ESP32 is loop() zelf een FreeRTOS-taak ("loopTask"). Al het werk
  // gebeurt in onze eigen taken, dus deze is niet nodig en verwijdert
  // zichzelf.
  vTaskDelete(nullptr);
}
