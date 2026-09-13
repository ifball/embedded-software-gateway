/*
 * STM32 + FreeRTOS migration sketch.
 *
 * Put this structure into your CubeMX project after UART, DMA, ADC/PWM
 * and FreeRTOS are enabled. Task names follow common embedded patterns:
 * SensorTask, ParserTask, ControlTask, CommTask, ReportTask.
 */

#include "protocol_port.h"

/*
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "main.h"
*/

typedef struct {
    uint16_t adc_value;
    uint8_t led_on;
    uint8_t pwm_percent;
    uint32_t error_count;
} device_state_t;

static device_state_t g_state;

/*
static QueueHandle_t g_rx_frame_queue;
static QueueHandle_t g_tx_frame_queue;
static SemaphoreHandle_t g_uart_idle_sem;
static SemaphoreHandle_t g_state_mutex;
static EventGroupHandle_t g_system_events;
*/

void SensorTask(void *argument) {
    (void)argument;
    for (;;) {
        /*
         * 1. Read ADC/I2C/SPI sensor.
         * 2. Lock g_state_mutex.
         * 3. Update g_state.adc_value.
         * 4. Unlock mutex.
         * 5. vTaskDelay(pdMS_TO_TICKS(200));
         */
    }
}

void ParserTask(void *argument) {
    (void)argument;
    for (;;) {
        /*
         * Wait for UART DMA + IDLE callback to give g_uart_idle_sem.
         * Move bytes from DMA buffer into RingBuffer.
         * Decode complete frames.
         * Send proto_frame_t into g_rx_frame_queue.
         */
    }
}

void ControlTask(void *argument) {
    (void)argument;
    for (;;) {
        /*
         * Receive proto_frame_t from g_rx_frame_queue.
         * Execute GET_STATUS / GET_SENSOR / SET_LED / SET_PWM.
         * Do not send UART bytes directly here.
         * Build response frame and send it to g_tx_frame_queue.
         */
    }
}

void CommTask(void *argument) {
    (void)argument;
    for (;;) {
        /*
         * Receive response frame from g_tx_frame_queue.
         * Encode with proto_encode().
         * Send by HAL_UART_Transmit_DMA or HAL_UART_Transmit.
         * Protect shared UART TX resource with a mutex if needed.
         */
    }
}

void ReportTask(void *argument) {
    (void)argument;
    for (;;) {
        /*
         * Optional: every 1s send status frame or update heartbeat.
         * Software timer callback should only notify this task,
         * not block on UART transmission.
         */
    }
}

void HAL_UARTEx_RxEventCallback(void *huart, uint16_t size) {
    (void)huart;
    (void)size;
    /*
     * BaseType_t higher_priority_task_woken = pdFALSE;
     * xSemaphoreGiveFromISR(g_uart_idle_sem, &higher_priority_task_woken);
     * portYIELD_FROM_ISR(higher_priority_task_woken);
     */
}
