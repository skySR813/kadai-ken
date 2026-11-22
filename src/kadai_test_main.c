#include <stdio.h>
#include <math.h>

#include "FreeRTOS.h"
#include "color_lcd.h"
#include "pico/stdlib.h"
#include "task.h"
#include "queue.h"
#include "tm1637.h"
#include "LSM6DSV16X.h"

#define SEVEN_SEGMENT1_PERIOD_MS 100
#define SEVEN_SEGMENT2_PERIOD_MS 100
#define LCD_PERIOD_MS 1
#define BUTTON_TASK_PERIOD_MS 5
#define TILT_SENSOR_PERIOD_MS 100

#define RADIANS_TO_DEGREES 57.2957795f

#define TM1637_CLK_PIN_1 6 
#define TM1637_DIO_PIN_1 7
#define TM1637_CLK_PIN_2 8
#define TM1637_DIO_PIN_2 9
#define SDA_PIN 0
#define SCL_PIN 1
#define BUTTON_PIN1 15
#define BUTTON_PIN2 14
#define BUTTON_PIN3 13

#define BUTTON_NUM 3

QueueHandle_t button_queue;


static lsm6dsv16x_t sensor;

static volatile int16_t tilt_x_degrees;
static volatile int16_t tilt_y_degrees;

static tm1637_config_t tm1637_config_1 = {
    .clk_pin = TM1637_CLK_PIN_1,
    .dio_pin = TM1637_DIO_PIN_1
};

static tm1637_config_t tm1637_config_2 = {
    .clk_pin = TM1637_CLK_PIN_2,
    .dio_pin = TM1637_DIO_PIN_2
};




typedef struct {
    const tm1637_config_t *display;
    TickType_t period;
    volatile int16_t *angle;
} seven_segment_task_config_t;


static seven_segment_task_config_t seven_segment_task_config_1 = {
    .display = &tm1637_config_1,
    .period = pdMS_TO_TICKS(SEVEN_SEGMENT1_PERIOD_MS),
    .angle = &tilt_x_degrees
};

static seven_segment_task_config_t seven_segment_task_config_2 = {
    .display = &tm1637_config_2,
    .period = pdMS_TO_TICKS(SEVEN_SEGMENT2_PERIOD_MS),
    .angle = &tilt_y_degrees
};

typedef struct { 
    uint8_t button_pin;
    bool is_pressed;
}button_Event_t;

static int16_t get_tilt_angle(const volatile int16_t *angle)
{
    int16_t value;
    taskENTER_CRITICAL();
    value = *angle;
    taskEXIT_CRITICAL();
    return value;
}

static void seven_segment_task(void *parameters)
{
    const seven_segment_task_config_t *config = parameters;
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        tm1637_display_number(config->display, get_tilt_angle(config->angle));
        vTaskDelayUntil(&last_wake, config->period);
    }
}

static void tilt_sensor_task(void *parameters)
{
    const lsm6dsv16x_t *imu = parameters;
    lsm6dsv16x_data_t readings;
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        if (lsm6dsv16x_read_sensor(imu, &readings)) {
            const float tilt_x = atan2f(readings.ay, readings.az) * RADIANS_TO_DEGREES;
            const float tilt_y = atan2f(-readings.ax, sqrtf(readings.ay * readings.ay + readings.az * readings.az)) * RADIANS_TO_DEGREES;
            taskENTER_CRITICAL();
            tilt_x_degrees = (int16_t)lroundf(tilt_x);
            tilt_y_degrees = (int16_t)lroundf(tilt_y);
            taskEXIT_CRITICAL();
        } else {
            printf("Failed to read LSM6DSV16X\n");
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TILT_SENSOR_PERIOD_MS));
    }
}

static void lcd_task(void *parameters)
{
    button_Event_t button_event;
    static const uint16_t lcd_colors1[] = {
        0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F
    };
    
    const size_t color_count = sizeof(lcd_colors1) / sizeof(lcd_colors1[0]);
    uint16_t count = 0;
    size_t color_index = 0;
    TickType_t last_wake = xTaskGetTickCount();
    color_lcd_display_number(count, lcd_colors1[color_index]);
    color_lcd_display_text("abcd",lcd_colors1[color_index]);

    (void)parameters;
    while (true) {
        if(xQueueReceive(button_queue, &button_event, portMAX_DELAY) == pdPASS){
            // Handle button event
            if(button_event.button_pin == BUTTON_PIN1 && button_event.is_pressed){
                color_index = (color_index + 1) % color_count;
                color_lcd_display_number(count, lcd_colors1[color_index]);
            } else if(button_event.button_pin == BUTTON_PIN2 && button_event.is_pressed){
                count = (count + 1) % 10000;
                color_lcd_display_number(count, lcd_colors1[color_index]);
            } else if(button_event.button_pin == BUTTON_PIN3 && button_event.is_pressed){
                count = (count - 1 + 10000) % 10000; // Decrement with wrap-around
                color_lcd_display_number(count, lcd_colors1[color_index]);
            }
        } 
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LCD_PERIOD_MS));
        
        //printf("%04u\n", (unsigned int)count);
        //color_lcd_display_number(count, lcd_colors1[color_index]);
        //count = (count + 1) % 10000;
        //color_index = (color_index + 1) % color_count;
        //vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LCD_PERIOD_MS));
        
    }
}

static void button_task(void *parameters)
{
    const uint8_t button_pins[] = {BUTTON_PIN1, BUTTON_PIN2, BUTTON_PIN3};
    button_Event_t button_events[BUTTON_NUM];
    TickType_t last_wake = xTaskGetTickCount();


    for (int i = 0; i < BUTTON_NUM; ++i) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
        button_events[i].button_pin = button_pins[i];
        button_events[i].is_pressed = true;
    }

    (void)parameters;
    while(true){
        for(int i = 0; i < BUTTON_NUM; ++i){
            bool current_state = !gpio_get(button_events[i].button_pin);
            if(current_state != button_events[i].is_pressed){
                button_events[i].is_pressed = current_state;
                xQueueSend(button_queue, &button_events[i], 0);
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(BUTTON_TASK_PERIOD_MS));
    }
}

int main(void)
{
    stdio_init_all();
    tm1637_init(&tm1637_config_1, TM1637_CLK_PIN_1, TM1637_DIO_PIN_1);
    tm1637_init(&tm1637_config_2, TM1637_CLK_PIN_2, TM1637_DIO_PIN_2);
    color_lcd_init();

    button_queue = xQueueCreate(10, sizeof(button_Event_t));
    if (button_queue == NULL) {
        printf("Failed to create button queue\n");
        while (true) {
            tight_loop_contents();
        }
    }

    if (!lsm6dsv16x_init(&sensor, i2c0, SDA_PIN, SCL_PIN, 400000)) {
        printf("Failed to initialize LSM6DSV16X\n");
        while (true) {
            tight_loop_contents();
        }
    }

    if (xTaskCreate(seven_segment_task, "seven-segment-1", 512, &seven_segment_task_config_1,tskIDLE_PRIORITY + 2, NULL) != pdPASS ||
        xTaskCreate(seven_segment_task, "seven-segment-2", 512, &seven_segment_task_config_2,tskIDLE_PRIORITY + 2, NULL) != pdPASS ||
        xTaskCreate(lcd_task, "lcd", 1024, NULL,tskIDLE_PRIORITY + 1, NULL) != pdPASS || 
        xTaskCreate(tilt_sensor_task, "tilt-sensor", 1024, &sensor,tskIDLE_PRIORITY + 3, NULL) != pdPASS ||
        xTaskCreate(button_task, "button", 512, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        printf("Failed to create tasks\n");
        while (true) {
            tight_loop_contents();
        }
    }

    vTaskStartScheduler();
    printf("FreeRTOS scheduler failed to start\n");
    while (true) {
        tight_loop_contents();
    }
}
