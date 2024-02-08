/*
 * Created: 06/02/2024 11:28:43
 * Author: BalazsFarkas 
 * Project: SAMD21_ClockDriver
 * Processor: SAMD21G18A 
 * File: clock.c
 * Program version: 1.0
 */ 


#include "clock.h"
#include "stdint.h"

/*
This is a basic clock setup code using bare metal.

At reset:
OSC8M is divided by 8 down to 1 MHz
GCLK0 (GCLK_MAIN) is on OSC8M
OSCULP32K is enabled and fed to GCLK2
0 FLASH wait cycles (NVCTRL->CTRLB.RWS = 0)
Instruction cache enabled (NVCTRL->CTRLB.CACHEDIS = 0)
CPU and APB is pre-scaled at 1
APBA drives EIC, RTC, WDT, GCLK, SYSCTRL, PM and PAC0

Steps:
1)Set FLASH wait cycles
2)Enable XOSC32K clock
3)Put XOSC32K as source for GCLK1
4)Put GCLK1 as source for Clock Mux 0
5)Enable DFLL48M clock
6)Switch GCLK0 to DFLL48M
7)Modify pre-scalers
8)Set CPU and  APB source as 48MHz


Note: we define generators/sources, GCLKs and then, clocks. The tricky part is that for the DFLL source to work, we need to first generate and run its reference clock.
We have 7 sources and we can generate 8 different GCLKs. We then have 37 different clocks (which technically will be the designated clocks for peripherals, timers, etc.)
Mind, we need to specify every time when we modify a clock, which "ID" we are working on within the GENDIV and the GENCTRL registers. The GCLKs don't have their designated registers.


Add timer and do proper blinky									DONE
Add SERCOM
Add SDcard
Attempt to replaced SD and sercom libraries in Bootmaster

*/

void ClocksInit(void){

	//on power up, the CPU's MAINCLK will be whatever is standard (here, 1 MHz from the OSC8M). It must be changed should it be necessary.
	
	uint32_t tempDFLL48CalibrationCoarse;				//coarse DFLL48 calibration taken from NVM
	
	//1)FLASH wait cycle

	NVMCTRL->CTRLB.bit.RWS = 1;							//FLASH wait cycle 1 - this must be set accordign to the clocking we do
	
	//2)Enable XOS32 to be the reference for the DFLL

	SYSCTRL_XOSC32K_Type sysctrl_xosc32k = {			//we give values to the union for the external oscillator
														//we technically load the bit values into a union and then copy that onto the register
														//we have a union type defined for all registers
														//of note, these type registers doesn't seem to exist for STM32
		
		.bit.WRTLOCK = 0,								//config not locked
		.bit.STARTUP = 0x2,								//3 cycles start-up time
		.bit.ONDEMAND = 0,								//always running when enabled - when ONDEMAND is HIGH, we will run the source only when the peripheral is asking for it
		.bit.RUNSTDBY = 0,								//disabled in standby and sleep
		.bit.AAMPEN = 0,								//disable automatic amplitude control
		.bit.EN32K = 1,									//32kHz output disabled
		.bit.XTALEN = 1									//connected to SIN32/XOUT32 pins
		
	};
	
	SYSCTRL->XOSC32K.reg = sysctrl_xosc32k.reg;			//publish settings
	
	SYSCTRL->XOSC32K.bit.ENABLE = 1;					//enable separately per recommendation
	
	while(!SYSCTRL->PCLKSR.bit.XOSC32KRDY);				//we wait until the clock is stable
	
	//3)Remove all pre-scaling from the GCLK1 generator
	
	GCLK_GENDIV_Type gclk1_gendiv = {
		
		.bit.DIV = 1,									//GCLK division factor
		.bit.ID = (1u)									//applies to GCLK1
		
	};
	
	GCLK->GENDIV.reg = gclk1_gendiv.reg;

	//4)Set up the GCLK1 generator with XOSC32K as source

	GCLK_GENCTRL_Type gclk1_genctrl = {					//CLK generator control
		
		.bit.RUNSTDBY = 0,								//stopped when standby
		.bit.DIVSEL = 0,								//pre-scale
		.bit.OE = 0,									//disable output to GCLK IO
		.bit.OOV = 0,		
		.bit.IDC = 1,									//50/50 duty cycle
		.bit.GENEN = 1,									//enable generator
		.bit.SRC = 0x05,								//XOS32K source
		.bit.ID = (1u)									//Gen ID: 1 - applies to GCLK1
		
	};
	
	GCLK->GENCTRL.reg = gclk1_genctrl.reg;
	while(GCLK->STATUS.bit.SYNCBUSY);					//synchronize
	
	//5)Set up the GCLK1 generator's mux and connect it to GCLK1
	
	GCLK_CLKCTRL_Type gclk_clkctrl = {					//CLK control
	
		.bit.WRTLOCK = 0,								//clock and generator and division factor are not locked
		.bit.CLKEN = 1,									//GCLK enabled
		.bit.GEN = (1u),								//choose generator, will be generator 1, so GCLK1
		.bit.ID = 0x0									//GCLK_DFLL48M_REF clock will be generated
		
	};
	
	GCLK->CLKCTRL.reg = gclk_clkctrl.reg;
	
	//6)Enable and calibrate the DFLL clock source
	
	while(!SYSCTRL->PCLKSR.bit.DFLLRDY);															//synchronization is complete flag (synch to reference clock). Must be checked before any work can be done to the DFLL.
	SYSCTRL->DFLLCTRL.reg = (uint16_t) (SYSCTRL_DFLLCTRL_ENABLE);									//we simply enable the DFLL
	while(!SYSCTRL->PCLKSR.bit.DFLLRDY);
	
	
	SYSCTRL_DFLLMUL_Type sysctrl_dfllmul = {
		
		.bit.CSTEP = 31,
		.bit.FSTEP = 511,
		.bit.MUL = 1465
		
	};
	
	SYSCTRL->DFLLMUL.reg = sysctrl_dfllmul.reg;														//we set the maximum step sizes allowed during coarse adjustment in closed-loop mode
	while(!SYSCTRL->PCLKSR.bit.DFLLRDY);
	
	tempDFLL48CalibrationCoarse = *(uint32_t*) FUSES_DFLL48M_COARSE_CAL_ADDR;
	tempDFLL48CalibrationCoarse &= FUSES_DFLL48M_COARSE_CAL_Msk;
	tempDFLL48CalibrationCoarse = tempDFLL48CalibrationCoarse>>FUSES_DFLL48M_COARSE_CAL_Pos;
	
	SYSCTRL->DFLLVAL.bit.COARSE = tempDFLL48CalibrationCoarse;										//we load the coarse calibration values into the DFLL clock module
	
	while(!SYSCTRL->PCLKSR.bit.DFLLRDY);
	SYSCTRL->DFLLCTRL.reg |= (uint16_t)(SYSCTRL_DFLLCTRL_MODE | SYSCTRL_DFLLCTRL_WAITLOCK);			//MODE is closed loop, output is active when DFLL is locked
	
	
	//7)Set up the GCLK0 generator with DFLL48M as source
	
	GCLK_GENCTRL_Type gclk_genctrl0 = {
		
		.bit.RUNSTDBY = 0,								//stopped when standby
		.bit.DIVSEL = 0,								//pre-scale
		.bit.OE = 1,									//enable output to GCLK IO
		.bit.OOV = 0,
		.bit.IDC = 1,									//50/50 duty cycle
		.bit.GENEN = 1,									//enable generator
		.bit.SRC = 0x07,								//DFLL48M source
		.bit.ID = (0u)									//Gen ID: 0
		
	};
	
	GCLK->GENCTRL.reg = gclk_genctrl0.reg;
	while(GCLK->STATUS.bit.SYNCBUSY);					//synchronize
	
	//8)Publish the DFLL48M clock to PA28

	PORT_WRCONFIG_Type port0_wrconfig = {				//we update 16 pins at the same time
		
		.bit.HWSEL = 1,									//upper half of the 32 pins
		.bit.WRPINCFG = 1,								//PINCFGy register update
		.bit.WRPMUX = 1,								//PMUX register update
		.bit.PMUX = 7,									//PMUX will be set to 7			-	peripheral function H selected
		.bit.PMUXEN = 1,								//new values to PMUXEN in the PINFCG register
		.bit.PINMASK = (uint16_t)(1<<28-16)				//specifically select the pins to update - here pin 20 in the upper bracket, so 20-16...
		
	};
	
	PORT->Group[0].WRCONFIG.reg = port0_wrconfig.reg;

	//9)Choose MAINCLK and APB at 48 MHz pre-scaling

	PM->CPUSEL.reg = PM_CPUSEL_CPUDIV_DIV1;
	PM->APBASEL.reg = PM_APBASEL_APBADIV_DIV1_Val;
	PM->APBBSEL.reg = PM_APBBSEL_APBBDIV_DIV1_Val;
	PM->APBCSEL.reg = PM_APBCSEL_APBCDIV_DIV1_Val;



	//10)OSC8M is already enabled when startup, we just need to remove the pre-scaler
	SYSCTRL->OSC8M.bit.PRESC = 0;
	SYSCTRL->OSC8M.bit.ONDEMAND = 0;	

}

//2)Set delay timer
void Delay_timer_config (void) {
	/**
	 * 16 bit basic timer.
	 * Enable power to the TC bus clock in PM
	 * CLKCTRL ID 0x1C is TC4/TC5, ID 0x1D is TC6/TC7 - paired as even an odd pairs
	 * TC4 is enabled on PM->APBCMASK.bit.TC4 - or bit 12
	 * TC4 is on PB08/A1 function "E"
	 * GCLK0 is GCLKMAIN. GCLKMux0 - GCLK0's mux output - is the reference for the DFLL
	 * GCLK is a prescaler (generator - GENCTRL.reg and GENDIV.reg - generator calibration) and the output (mux - CLKCTRL.reg - these are the peripheral channel calibration) 
	 * unlike STM32 where the peripheral is clocked from APB/AHB, in SAMD21 APB/AHB are busses clocked separately than the peripheral. The peri is synched to the bus by hardware. As such, the interface is clocked differently than the peripheral.
	 * APBx, AHB and MAINCLK are all derivatives of the same clock, thus are synchronous. AHB can't be prescaled, APBx can be. Thus, we have one clock domain for MAINCLK and AHB, and another for each APB.

	 **/
	
	//set the clock generator - 16.2.2.1

	//1)Pre-scale for GCLK5

	GCLK_GENDIV_Type gclk5_gendiv = {
		
		.bit.DIV = 8,									//pre-scaling by 8 on GCLK5
		.bit.ID = (5u)
		
	};
	
	GCLK->GENDIV.reg = gclk5_gendiv.reg;				//this must be a 32-bit write

	//2)Setup for GCLK5

	GCLK_GENCTRL_Type gclk5_genctrl = {
		
		.bit.RUNSTDBY = 0,								//stopped when standby
		.bit.DIVSEL = 0,								//normal division
		.bit.OE = 0,									//enable output to GCLK IO
		.bit.OOV = 0,
		.bit.IDC = 1,									//50/50 duty cycle
		.bit.GENEN = 1,									//enable generator - each GCLK enable separately!
		.bit.SRC = 0x06,								//OSC8M source - changing can be done on the fly!
		.bit.ID = (5u)									//Gen ID: 5
		
	};
	
	//Note: with DIVSEL set to 0 and DIV to 8, the GCLK should be running at 8000000 Hz / 8 = 1 MHz

	GCLK->GENCTRL.reg = gclk5_genctrl.reg;				//this must be a 32-bit write
	while(GCLK->STATUS.bit.SYNCBUSY);

	//3)Set the muxing towards the TC4/5 peripheral

	GCLK_CLKCTRL_Type gclk_clkctrl = {					//CLK control
		
		.bit.WRTLOCK = 0,								//clock and generator and division factor are not locked
		.bit.CLKEN = 1,									//clock channel enabled
		.bit.GEN = (5u),								//choose generator, will be generator 1, so GCLK1 - can not be hot-swapped
		.bit.ID = 0x1C									//choose channel: TC4/TC5 clock will be generated
		
	};
	
	GCLK->CLKCTRL.reg = gclk_clkctrl.reg;				//this must be a 16-bit write
	while(GCLK->STATUS.bit.SYNCBUSY);					//we wait until synch is done
	

	//4)Enable the APBx interface towards the TC4/5 peripheral
	//Note: APB bus clocking is already enabled on startup (CLK_SYSCTRL_APB)

	PM->APBCMASK.bit.TC4_ = 1;

	//5)Set the muxing towards the TC4/5 peripheral

	TC4->COUNT16.CTRLA.bit.ENABLE = 0;					//TC disabled
	TC4->COUNT16.CTRLA.bit.MODE = 0;					//counter16
	TC4->COUNT16.CTRLBSET.bit.DIR = 0;					//counting up
	TC4->COUNT16.COUNT.reg = (0u);						//we reset the counter

	TC4->COUNT16.CTRLA.bit.ENABLE = 1;					//TC enabled
	while(TC4->COUNT16.STATUS.bit.SYNCBUSY);			//synch

}



//3) Delay function for microseconds
void Delay_in_us(uint16_t micro_sec) {
	/**
	 * 1)Reset counter for TC4
	 * 2)Wait until micro_sec
	 **/

	TC4->COUNT16.COUNT.reg = (0u);						//we reset the counter
	while((TC4->COUNT16.COUNT.reg) < micro_sec);		//Note: this is a blocking timer counter!
}


//4) Delay function for milliseconds
void Delay_in_ms(uint16_t milli_sec) {
	for (uint32_t i = 0; i < milli_sec; i++){
		Delay_in_us(1000);								//we call the custom microsecond delay for 1000 to generate a delay of 1 millisecond
	}
}