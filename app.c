/*
 * Created: 06/02/2024 09:46:27
 * Author: BalazsFarkas
 * Project: SAMD21_ClockDriver
 * Processor: SAMD21G18A
 * File: app.c
 * Program version: 1.0
 */ 


#include "app.h"
#include "clock.h"

void AppInit(void) {
	
	ClocksInit();															//initialize device clock

	PORT->Group[0].DIR.reg |= (1<<17); 

	PORT->Group[0].OUTCLR.reg |= (1<<17);									//set LED pin as LOW

	Delay_timer_config();													//initialize delay function's clock

}

void AppRun(void) {
	
	while(1)
	{

		PORT->Group[0].PINCFG[(1<<17)].bit.DRVSTR = 1;						//set drive strength for pin (to have maximum current sourced from the pin) - if not set, maximum current is only 2 mA!

		PORT->Group[0].OUTSET.reg |= (1<<17);								//turn on LED

		Delay_in_ms(1000);													//wait for 1000 ms

		PORT->Group[0].OUTCLR.reg |= (1<<17); 								//turn off LED

		Delay_in_ms(1000);													//wait for 1000 ms
	
	}
}
	