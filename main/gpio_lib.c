/** ************************************************************************************************
 *	GPIO management library
 *  (c) Fernando R (iambobot.com)
 *
 *  1.0.0 - February 2026

 *
 ** ************************************************************************************************
**/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "config.h"
#include "gpio_lib.h"

/**
---------------------------------------------------------------------------------------------------
		
								   GPIO OUTPUT: ON-BOARD LED & RELAYS

---------------------------------------------------------------------------------------------------
**/

// https://github.com/espressif/esp-idf/tree/v5.5.2/examples/peripherals/gpio/generic_gpio

#define GPIO_OUTPUT_PIN_SEL  		((1ULL<<PIN_GPIO_ONBOARD_LED) | (1ULL<<PIN_GPIO_RELAY_1) | (1ULL<<PIN_GPIO_RELAY_2))

void gpiolib_output_init(void)
{
    //zero-initialize the config structure
    gpio_config_t io_conf_out = {};
    //disable interrupt
    io_conf_out.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf_out.mode = GPIO_MODE_OUTPUT; // GPIO_MODE_OUTPUT / GPIO_MODE_OUTPUT_OD
    //bit mask of the pins that you want to set
    io_conf_out.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf_out.pull_down_en = 0;
    //disable pull-up mode
    io_conf_out.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf_out);	
} // gpiolib_output_init


/**
---------------------------------------------------------------------------------------------------
		
								   GPIO INPUT: ON-BOARD BUTTON

---------------------------------------------------------------------------------------------------
**/
#define GPIO_INPUT_PIN_SEL  		(1ULL<<PIN_GPIO_ONBOARD_BUTTON)
#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t gpio_evt_queue = NULL;

static uint32_t ISR_count= 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
	ISR_count ++;
} // gpio_isr_handler

static void gpio_BUTTON_handle_task(void* f)
{
	void (*g)(int)= f;
    uint32_t io_num;
    for (;;) 
	{
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))			
		{
			uint32_t gpio_value= gpio_get_level(PIN_GPIO_ONBOARD_BUTTON);
			printf("\nGPIO[%"PRIu32"] intr, val: %d", io_num, gpio_get_level(io_num));
			if(g) g(gpio_value);
        }
    }
} // gpio_BUTTON_handle_task

void gpiolib_input_init(void (*f)(int), UBaseType_t uxPriority)
{
    //zero-initialize the config structure
    gpio_config_t io_conf = {};
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
	
    //change gpio interrupt type for one pin
	gpio_set_intr_type(PIN_GPIO_ONBOARD_BUTTON, GPIO_INTR_ANYEDGE);

	// create a queue to handle gpio event from isr
	gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    // start gpio task
	if( gpio_evt_queue != 0 )
	{
		xTaskCreate(
			gpio_BUTTON_handle_task, 
			"gpio_task", 
			8*1024,
			(void*)f,
			uxPriority, 
			NULL);
	}

	//install gpio isr service
	ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
	//hook isr handler for specific gpio pin
	ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_GPIO_ONBOARD_BUTTON, gpio_isr_handler, (void*) PIN_GPIO_ONBOARD_BUTTON));
} // gpiolib_output_init


// END OF FILE