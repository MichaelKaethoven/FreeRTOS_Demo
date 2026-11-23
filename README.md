# FreeRTOS-demo – ESP32

Een klein voorbeeld van FreeRTOS-taken op de ESP32 (DFRobot FireBeetle 2 ESP32-E, Arduino-framework, PlatformIO).

- **Deel 1** is de minimale werkende code: twee taken die onafhankelijk van elkaar draaien.
- **Deel 2** voegt drie optionele functies toe: een queue, een mutex en een stackmonitor. Je zet ze aan bovenaan in `src/main.cpp`.

## Bouwen en uploaden

```
pio run -t upload
pio device monitor -b 115200
```

Hardware: de ingebouwde LED (GPIO 2). Voor de queue-uitbreiding is ook een drukknop tussen GPIO 27 en GND nodig.

## Deel 1 – minimale code

| Taak | Wat ze doet | Ritme |
|---|---|---|
| `blinkTask` | Schakelt de LED aan/uit | elke 500 ms |
| `counterTask` | Drukt een teller af op Serial | elke 1000 ms |

Geen van beide taken wacht op de andere. Terwijl de ene taak slaapt in `vTaskDelay()`, geeft FreeRTOS de CPU aan de andere. `loop()` is niet nodig en verwijdert zichzelf.

Verwachte uitvoer:

```
FreeRTOS demo starting
[counter] 0 (running on core 1)
[counter] 1 (running on core 1)
...
```

## Deel 2 – optionele uitbreidingen

Zet een schakelaar op `1` bovenaan in `main.cpp`:

| Schakelaar | Wat ze toont |
|---|---|
| `ADD_QUEUE` | Een **queue** (wachtrij). `buttonTask` leest de knop uit en stuurt het nummer van de druk naar `buttonHandlerTask`, die op data wacht zonder CPU-tijd te gebruiken. Zo geef je veilig data door tussen taken. |
| `ADD_MUTEX` | Een **mutex**. Maar één taak tegelijk mag `Serial` gebruiken, zodat een blok van meerdere regels nooit door een andere taak onderbroken wordt. |
| `ADD_MONITOR` | Een **monitortaak** op core 0. Elke 5 s drukt ze af hoeveel stack elke taak nog over heeft en hoeveel heap er vrij is. Ze toont dat taken op beide cores kunnen draaien en helpt je de stackgroottes te kiezen. |

De uitbreidingen werken elk apart, maar `ADD_MUTEX` is het best zichtbaar samen met `ADD_MONITOR`, die een blok van 5 regels afdrukt. Arduino's `Serial` vergrendelt elke afzonderlijke aanroep al, dus één regel raakt nooit door elkaar. Alleen een groep regels heeft de mutex nodig.

## Belangrijke begrippen

- **Taak (task):** een functie met een eindeloze `for (;;)`-lus, een eigen stack en een prioriteit. Vergelijkbaar met een thread. Een taak mag nooit `return`en; ze stopt met `vTaskDelete()`.
- **Scheduler:** FreeRTOS beslist welke taak draait. Hij is *preemptive*: een taak met een hogere prioriteit onderbreekt een lagere, en taken met dezelfde prioriteit wisselen elkaar af (time-slicing, tick van 1 ms).
- **Twee cores:** de ESP32 heeft core 0 (vooral Wi-Fi/Bluetooth) en core 1 (waar `loop()` draait). Met `xTaskCreatePinnedToCore()` kies je de core.
- **`vTaskDelay()`:** laat alleen die ene taak slapen. De rest van het programma blijft draaien, dus dit is geen blokkerende code.
- **Gedeelde data:** taken kunnen op hetzelfde moment draaien, dus twee taken die dezelfde variabele aanpassen kunnen botsen. Gebruik een queue om data door te geven en een mutex om een gedeeld hulpmiddel te beschermen.

## FreeRTOS vs TaskScheduler

Mijn hoofdproject (de wijnkeldermonitor) gebruikt de TaskScheduler-bibliotheek voor de timing. Het verschil:

| | TaskScheduler | FreeRTOS |
|---|---|---|
| Soort | Coöperatief: callbacks worden na elkaar aangeroepen vanuit `loop()` | Preemptive: echte taken, zoals threads |
| Parallel | Nee, één thread | Ja, time-slicing en twee cores |
| Een trage callback | Blokkeert alle andere taken | Alleen die taak wacht |
| Geheugen | Heel weinig per taak | Eigen stack per taak (2–12 kB) |
| Gedeelde variabelen | Veilig, er draait nooit iets tegelijk | Queue of mutex nodig |
| Geschikt voor | Korte taken "elke X ms" | Lang wachten, zoals een HTTPS-verzoek |

In het wijnkelderproject worden beide gebruikt. TaskScheduler regelt de metingen, de LEDs, de knop en de heartbeat. Een FreeRTOS-taak verstuurt de e-mailwaarschuwing, omdat dat HTTPS-verzoek 1–2 seconden duurt en anders al de rest zou laten vastlopen.

## Bronnen

- FreeRTOS op de ESP32 (ESP-IDF): https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos.html
- FreeRTOS API-referentie: https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/00-TaskHandle
- TaskScheduler: https://github.com/arkhipenko/TaskScheduler
