/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>


#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

const int BTN_PIN_R = 28;
const int BTN_PIN_Y = 21;

const int LED_PIN_R = 5;
const int LED_PIN_Y = 10;

// Únicas globais permitidas: recursos do RTOS
QueueHandle_t xQueueBtn;          // btn_callback -> btn_task (qual botão foi apertado)
SemaphoreHandle_t xSemaphoreLedR; // btn_task -> led_r_task (alterna pisca/apaga)
SemaphoreHandle_t xSemaphoreLedY; // btn_task -> led_y_task (alterna pisca/apaga)

// ISR: só descobre qual botão foi e manda o número do pino pela fila.
// Nada de printf ou lógica aqui dentro.
void btn_callback(uint gpio, uint32_t events) {
    if (events == GPIO_IRQ_EDGE_FALL) {
        int id = (int)gpio;
        xQueueSendFromISR(xQueueBtn, &id, NULL);
    }
}

// Recebe os eventos da ISR e decide qual LED avisar
void btn_task(void* p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);

    // Um callback para os dois botões (o Pico só aceita um callback de GPIO)
    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    // Debounce: ignora eventos do mesmo botão muito próximos
    TickType_t last_r = 0;
    TickType_t last_y = 0;
    const TickType_t debounce = pdMS_TO_TICKS(200);

    int id = 0;
    while (true) {
        if (xQueueReceive(xQueueBtn, &id, portMAX_DELAY) == pdTRUE) {
            TickType_t now = xTaskGetTickCount();

            if (id == BTN_PIN_R && (now - last_r) > debounce) {
                last_r = now;
                xSemaphoreGive(xSemaphoreLedR);
            } else if (id == BTN_PIN_Y && (now - last_y) > debounce) {
                last_y = now;
                xSemaphoreGive(xSemaphoreLedY);
            }
        }
    }
}

// LED R: cada "give" no semáforo alterna entre piscando e apagado
void led_r_task(void* p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 0);

    int blinking = 0;
    while (true) {
        if (!blinking) {
            // Apagado: dorme até alguém apertar o botão
            if (xSemaphoreTake(xSemaphoreLedR, portMAX_DELAY) == pdTRUE) {
                blinking = 1;
            }
        } else {
            // Piscando: checa sem bloquear se apertaram de novo
            if (xSemaphoreTake(xSemaphoreLedR, 0) == pdTRUE) {
                blinking = 0;
                gpio_put(LED_PIN_R, 0); // para e fica apagado
            } else {
                gpio_put(LED_PIN_R, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_put(LED_PIN_R, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }
}

// LED Y: mesma lógica da led_r_task, com o semáforo amarelo
void led_y_task(void* p) {
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);
    gpio_put(LED_PIN_Y, 0);

    int blinking = 0;
    while (true) {
        if (!blinking) {
            if (xSemaphoreTake(xSemaphoreLedY, portMAX_DELAY) == pdTRUE) {
                blinking = 1;
            }
        } else {
            if (xSemaphoreTake(xSemaphoreLedY, 0) == pdTRUE) {
                blinking = 0;
                gpio_put(LED_PIN_Y, 0);
            } else {
                gpio_put(LED_PIN_Y, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_put(LED_PIN_Y, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }
}


int main() {
    stdio_init_all();

    // Recursos do RTOS criados antes das tasks
    xQueueBtn = xQueueCreate(32, sizeof(int));
    xSemaphoreLedR = xSemaphoreCreateBinary();
    xSemaphoreLedY = xSemaphoreCreateBinary();

    xTaskCreate(btn_task, "BTN_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(led_r_task, "LED_R_Task", 256, NULL, 1, NULL);
    xTaskCreate(led_y_task, "LED_Y_Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while(1){}

    return 0;
}