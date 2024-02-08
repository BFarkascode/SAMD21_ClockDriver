/*
 * Created: 06/02/2024 11:28:09
 * Author: BalazsFarkas
 * Project: SAMD21_ClockDriver
 * Processor: SAMD21G18A
 * File: clock.h
 * Header version: 1.0
 */ 


#ifndef CLOCK_H_
#define CLOCK_H_

#include "sam.h"

////Function prototypes////
void ClocksInit(void);
void Delay_timer_config (void);
void Delay_in_us(uint16_t micro_sec);
void Delay_in_ms(uint16_t milli_sec);


#endif /* CLOCK_H_ */