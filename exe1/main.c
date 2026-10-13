#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include <stdio.h>

const int LED_PIN_R = 4;
const int LED_PIN_G = 6;

// Task do LED vermelho: pisca com período de 500 ms (250 ms aceso, 250 ms apagado)
void led_1_task(void *p) {
  gpio_init(LED_PIN_R);
  gpio_set_dir(LED_PIN_R, GPIO_OUT);

  int delay = 250;
  while (true) {
    gpio_put(LED_PIN_R, 1);
    vTaskDelay(pdMS_TO_TICKS(delay));
    gpio_put(LED_PIN_R, 0);
    vTaskDelay(pdMS_TO_TICKS(delay));
  }
}

// Task do LED verde: mesma lógica, mas independente da task do vermelho.
// Usa um delay diferente para ficar visível que as duas rodam "em paralelo".
void led_2_task(void *p) {
  gpio_init(LED_PIN_G);
  gpio_set_dir(LED_PIN_G, GPIO_OUT);

  int delay = 500;
  while (true) {
    gpio_put(LED_PIN_G, 1);
    vTaskDelay(pdMS_TO_TICKS(delay)); // libera a CPU para as outras tasks
    gpio_put(LED_PIN_G, 0);
    vTaskDelay(pdMS_TO_TICKS(delay));
  }
}

int main() {
  stdio_init_all();
  printf("Start RTOS \n");

  // Cada LED tem a sua própria task, com a mesma prioridade (1)
  xTaskCreate(led_1_task, "LED_Task 1", 256, NULL, 1, NULL);
  xTaskCreate(led_2_task, "LED_Task 2", 256, NULL, 1, NULL);

  // A partir daqui quem manda é o escalonador; o código abaixo não executa
  vTaskStartScheduler();

  while (true)
    ;
}