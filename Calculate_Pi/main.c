/*
 * Calculate_Pi.c
 *
 * Created: 20.03.2018 18:32:07
 * Author : chaos
 */ 

//#include <avr/io.h>
#include "stdio.h"
#include "stdlib.h"
#include "avr_compiler.h"
#include "pmic_driver.h"
#include "TC_driver.h"
#include "clksys_driver.h"
#include "sleepConfig.h"
#include "port_driver.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "stack_macros.h"

#include "mem_check.h"

#include "init.h"
#include "utils.h"
#include "errorHandler.h"
#include "NHD0420Driver.h"
#include "ButtonHandler.h"


extern void vApplicationIdleHook( void );
void vLedBlink(void *pvParameters);
void my_calculation(void *pvParameters);
void my_display(void *pvParameters);
void my_button_handler(void *pvParameters);
void vTimeTask(void *pvParameters);



TaskHandle_t ledTask;
TaskHandle_t calculate_pi;
TaskHandle_t display_task;
TaskHandle_t button_handler;
//TaskHandle_t my_time_task;
EventGroupHandle_t xCreatedEventGroup;

EventBits_t my_event_byte;
#define BIT0 (1<<0) //00000001
#define BIT1 (1<<1)	//00000010
#define BIT2 (1<<2) //00000100
#define BIT3 (1<<3)
#define BIT4 (1<<4)
#define BIT5 (1<<5)
#define BIT6 (1<<6)
#define BIT7 (1<<7)
#define BIT8 (1<<8)

float dPi4 = 1.0;
float zahl_pi = 3.1416/4 ;
int button_pressed;
int event_status;
int count = 0;
uint16_t buttonstate = 0x0000;

void vApplicationIdleHook( void )
{	
	
}

int main(void)
{
    resetReason_t reason = getResetReason();
	vInitClock();
	vInitDisplay();
	xTaskCreate( vLedBlink, (const char *) "ledBlink", configMINIMAL_STACK_SIZE+10, NULL, 1, &ledTask);
	xTaskCreate( my_calculation, (const char *) "ledBlink", configMINIMAL_STACK_SIZE+10, NULL, 1, &calculate_pi);
	xTaskCreate( my_display, (const char *) "ledBlink", configMINIMAL_STACK_SIZE+10, NULL, 1, &display_task);
	xTaskCreate( my_button_handler, (const char *) "ledBlink", configMINIMAL_STACK_SIZE+10, NULL, 1, &button_handler);
	xCreatedEventGroup = xEventGroupCreate();
	vTaskStartScheduler();
	return 0;
}

void vLedBlink(void *pvParameters){
	(void) pvParameters;
	PORTF.DIRSET = PIN0_bm; /*LED1*/
	PORTF.OUT = 0x00;
	for(;;) 
	{
		PORTF.OUTTGL = 0x01;				
		vTaskDelay(500 / portTICK_RATE_MS);
	}
}
void my_calculation(void *pvParameters){
	float n = 0.0;
	int i = 0;
	for (;;)
	{
		my_event_byte = xEventGroupWaitBits(xCreatedEventGroup,BIT0|BIT1,pdFALSE,pdFALSE,portMAX_DELAY);	
		if ((my_event_byte & BIT0) != 0)							//wen Event Bit0 gesetzt ist..
		{
			for(i=0;i<200000;i++)																	//anzahl wiederholungen
			{	
					dPi4 = dPi4 - 1/(3+n*4)+1/(5+n*4);	//dmyPi - 1/(3+i*4) + 1/(5+i*4);			//Berechnung von Pi
					n++;	
					//vDisplayWriteStringAtPos(3,0,"forschleife %d",i);
					if (zahl_pi >= dPi4)
					{
						TCD0.CTRLA = 0x00 ;
						xEventGroupClearBits(xCreatedEventGroup,BIT0);	
					}
			}
		}
		if ((my_event_byte & (BIT2)) != 0 )
			{
				dPi4 = 1;
				//current_result = 0.0;
				n = 0;
				my_event_byte = xEventGroupClearBits(xCreatedEventGroup,BIT2);
			}
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}
void my_button_handler(void *pvParameters){									//Tasten einlese Funktion
	int button_1_state,button_2_state = 0;

	for (;;)
	{
		updateButtons();
		
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {						//wenn button 1 kurz gedrückt
			if (button_1_state == 1)
			{
				xEventGroupClearBits(xCreatedEventGroup,BIT0);				//Bit 0 wird auf 0 gesestzt -> berechnung wird gestoppt
				TCD0.CTRLA = 0x00 ;
				button_1_state = 0;
			}
			else
			{
				xEventGroupSetBits(xCreatedEventGroup,BIT0);				//Bitt0 wirt auf 1 gesetzt -> berechnung wird gestartet
					TCD0.CTRLA = TC_CLKSEL_DIV1024_gc ;
					TCD0.CTRLB = 0x00;
					TCD0.INTCTRLA = 0x03;
					TCD0.PER = 312.5-1;
				button_1_state = 1;
				button_pressed = 1;
			}
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {
			if (button_2_state == 1)
			{
				xEventGroupClearBits(xCreatedEventGroup,BIT1);				//Bit 1 wird auf 0 gesestzt -> Zwischenresultat wird nicht angezeigt
				button_2_state = 0;
			}
			else
			{
				my_event_byte = xEventGroupSetBits(xCreatedEventGroup,BIT1);		// Bit 1 wird auf 1 gesestzt -> Zwischenresultat wird nicht angezeigt
				button_2_state = 1;
				button_pressed = 2;
			}
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {		
			
			my_event_byte = xEventGroupSetBits(xCreatedEventGroup,BIT2);			// Bit 2 wird auf 1 gesestzt -> berechnung wird gestoppt
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {
				xEventGroupClearBits(xCreatedEventGroup,BIT0|BIT1|BIT2|BIT3);		//Clear bits, setzt alles auf 0  -> berechnung reset
				button_pressed = 4;
				button_1_state = 0;
				button_2_state = 0;
		}
		vTaskDelay((200/BUTTON_UPDATE_FREQUENCY_HZ)/portTICK_RATE_MS);
	}
	
}
	
void my_display(void *pvParameters){												//display anzeige funktion
		char* my_result;
		char* Zaehler;
		
				vDisplayClear();
		for (;;)
		{
			dtostrf(dPi4*4,1,9,my_result);								//float in string umwandeln
			vDisplayWriteStringAtPos(0,0,"Calculate Pi");
			vDisplayWriteStringAtPos(1,0,"Cal %s:",my_result);
			vDisplayWriteStringAtPos(2,0,"Event Bit: %d",event_status);
			vDisplayWriteStringAtPos(3,0,": %d ms",count);	
			event_status = my_event_byte;								//zuweisen des Eventstatus nach event_status, das wird zur kontrolle auf dem Display ausgegeben	
			xEventGroupGetBits(my_event_byte);	
			
		if ((my_event_byte & BIT1) != 0 )
			{
				vDisplayWriteStringAtPos(3,0,"zw: %s",my_result);
				my_event_byte = xEventGroupClearBits(xCreatedEventGroup,BIT1);
			}

		vTaskDelay(200 / portTICK_RATE_MS);
		}
		
}

ISR(TCD0_OVF_vect)
{
	PORTF.DIRSET = PIN1_bm; /*LED1*/
	//PORTF.OUT = 0x01;
	count ++;
	PORTF.OUTTGL = 0x02;
	TCD0.CTRLFSET = TC_CMD_RESTART_gc;
	
}