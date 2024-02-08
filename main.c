/*
 * Created: 06/02/2024 13:46:04
 * Author: BalazsFarkas
 * Project: SAMD21_ClockDriver
 * Processor: SAMD21G18A
 * Compiler: ARM-GCC (Atmel Studio 7.0)
 * File: main.c
 * Program version: 1.0
 * Program description: Blinky
 * Hardware description/pin distribution:
											LED on D13/PA17
 * Change history:
 */ 


////Includes////
#include "sam.h"											//this comes with the compiler
#include "app.h"


////Function Prototypes////
void ClocksInit(void);										//configure clocking

int main(void)
{
	
	AppInit();

	while (1)
	{
		AppRun();
	}
}
