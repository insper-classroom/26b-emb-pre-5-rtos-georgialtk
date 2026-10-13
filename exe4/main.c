#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include <stdio.h>

const int BTN_PIN_R = 28;
const int BTN_PIN_G = 26;

const int LED_PIN_R = 4;
const int LED_PIN_G = 6;

QueueHandle_t xQueueButId;   // btn_1_task -> led_1_task (delay do LED R)
SemaphoreHandle_t xSemaphore_r; // ISR -> btn_1_task (clique no BTN R)

QueueHandle_t xQueueBtnG;    // ISR -> btn_2_task (clique no BTN G)
QueueHandle_t xQueueButId2;  // btn_2_task -> led_2_task (delay do LED G)

// Callback ÚNICO para todos os botões (o Pico só aceita um callback de GPIO).
// O parâmetro gpio diz qual pino gerou a interrupção.
void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) { // fall edge
        if (gpio == BTN_PIN_R) {
            xSemaphoreGiveFromISR(xSemaphore_r, 0);
        } else if (gpio == BTN_PIN_G) {
            // Dentro de IRQ só pode usar as funções "FromISR"
            int id = BTN_PIN_G;
            xQueueSendFromISR(xQueueBtnG, &id, 0);
        }
    }
}

void led_1_task(void *p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);

    int delay = 0;

    while (true) {
        if (xQueueReceive(xQueueButId, &delay, 0)) {
            printf("%d\n", delay);
        }

        if (delay > 0) {
            gpio_put(LED_PIN_R, 1);
            vTaskDelay(pdMS_TO_TICKS(delay));
            gpio_put(LED_PIN_R, 0);
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
    }
}

void btn_1_task(void *p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);
    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);

    int delay = 0;
    while (true) {
        if (xSemaphoreTake(xSemaphore_r, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (delay < 1000) {
                delay += 100;
            } else {
                delay = 100;
            }
            printf("delay btn %d \n", delay);
            xQueueSend(xQueueButId, &delay, 0);
        }
    }
}

// LED G: igual à led_1_task, mas lendo a fila do verde
void led_2_task(void *p) {
    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);

    int delay = 0;

    while (true) {
        if (xQueueReceive(xQueueButId2, &delay, 0)) {
            printf("%d\n", delay);
        }

        if (delay > 0) {
            gpio_put(LED_PIN_G, 1);
            vTaskDelay(pdMS_TO_TICKS(delay));
            gpio_put(LED_PIN_G, 0);
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
    }
}

// BTN G: em vez de semáforo, espera o aviso da ISR chegar pela fila xQueueBtnG
void btn_2_task(void *p) {
    gpio_init(BTN_PIN_G);
    gpio_set_dir(BTN_PIN_G, GPIO_IN);
    gpio_pull_up(BTN_PIN_G);
    // Mesmo callback: registrar de novo é seguro e garante que ele já existe
    // quando a IRQ do verde for habilitada
    gpio_set_irq_enabled_with_callback(BTN_PIN_G, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);

    int delay = 0;
    int id = 0;
    while (true) {
        if (xQueueReceive(xQueueBtnG, &id, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (delay < 1000) {
                delay += 100;
            } else {
                delay = 100;
            }
            printf("delay btn g %d (gpio %d)\n", delay, id);
            xQueueSend(xQueueButId2, &delay, 0);
        }
    }
}

int main() {
    stdio_init_all();
    printf("Start RTOS \n");

    xQueueButId = xQueueCreate(32, sizeof(int));
    xSemaphore_r = xSemaphoreCreateBinary();

    xQueueBtnG = xQueueCreate(32, sizeof(int));
    xQueueButId2 = xQueueCreate(32, sizeof(int));

    xTaskCreate(led_1_task, "LED_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(btn_1_task, "BTN_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(led_2_task, "LED_Task 2", 256, NULL, 1, NULL);
    xTaskCreate(btn_2_task, "BTN_Task 2", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}