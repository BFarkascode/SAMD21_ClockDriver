# SAMD21_ClockDriver
Bare metal clock driver on a SAMD21 with a basic delay function and blinky.

## General description
It’s blinky… how hard can it be on a SAMD21 without using the Arduino environment? It literally is the most basic code one can write, so it must be pretty simple…RIGHT?

Well…now that I have managed to successfully crawl out of the seven depths of the SAMD21’s clock driver, I have decided to make a project on this. The project isn’t particularly complicated – it’s a blinky – but I want to give detail explanations over how the clocks are handled on this device.

Because the clock system is an absolute PAIN on Atmel’s ARM devices! The main problem is that everything runs on pretty much its own clock domain, which needs to be set up, enabled, then synchronized to ensure good functionality. Failing to do so there will be no direct communication between the CPU, the associated communication busses and the peripherals.

Below I will present a project to do a blinky with a custom clock setup. I will also publish the 48 MHz clock on an IO to validate clocking using an oscilloscope.

## Previous relevant projects
None.

## To read
Getting familiar with multiple chapters of the SAMD21 datasheet is a MUST.
It is necessary to go through:
- Clock systems – to understand the clock hierarchy, how the different elements relate to each other, why we need to synchronize all the time, what are the different clock domains in the SAM
- Generic Clock Controller – to understand what the clock module (which is the clock generator AND the clock multiplexer/channels together) does, how it needs to be set up and connected to clock sources and how the module clocks peripherals, what we need to enable
- Power Management – to understand how the interface between the CPU and the peripherals (the APBx and AHB buses) are clocked and why, what we need to enable
- System control – to understand, how to set up the clock sources and what we need to enable

I also recommend reading up on clock domain crossing on just to get a hang of why it is so problematic that we have almost everything in the SAM on different – mostly asynchronous – clock domains.

## Particularities
1) Unlike STM32’s where we have one clock source which then branches off into AHB/APBx clocks that directly drive the peripheral and the interface (with local pre-scalers within the peripherals themselves), peripherals are clocked on a separate clock domain completely through the GCLK channels. The intrefaces all have their own pre-scalers which are NOT set in the clocking section, but within the power management (PM->CPUSEL.reg, for instance).
2) The CPU’s and the AHB/APBx clocks are shared. They all have the same source, so they are already synched.
3) The interface/CPU may not be synched to the peripheral. They must be synched prior to any application during initialization otherwise we may not be able to write to peripheral.
4) The naming convention of the SAMD21 is a confusing nightmare. Clock generators are set up by the GENCLK registers, while the entire module is called GCLK – short for Generic Clock (so GENCLK is not Generic Clock, it is Generic Clock Generator…you get the picture). GCLK multiplexers – which names are rarely used except for a few images – are simply called as “CLK” in the registers. Personally, I would have preferred GCLKGEN as name for generator registers and GCLKMUX for the multiplexer…
5) The GCLK generators are not treated separately but as one 32 bit register. We choose between which GCLK we intend to interact with by modifying the “ID” bits within the “GENXXX” registers. Mind, the ID bits in the “CLKXXX” registers will be for the mux output, despite both being handled within the GCLK module. So the two IDs are not referring to the same thing…
6) GCLK0 is special. The output of the GCLK0 generator’s output – so the signal that leaves the generator, BOT the multiplexer - IS the MAINCLK for the CPU, independent of what kind of muxing we do with the output afterwards. On hardware reset, this will be set as 1 MHz with the internal 8MHz oscillator as source divided up by 8.
7) The datasheet suggests a one-time 32-bit write to the registers of the clocks. Thus we won’t be flipping bits one-by-one, but instead rely on the unions coming with the “sam.h” package. Mind, these packages are unique to the SAM devices and – to my knowledge - do not exist on STM32.

## Explanations
### Setting up DFLL48M
DFLL48 – which is probably the go-to clock people wish to use on a SADM21 – is complicated to set up.

It has its own reference clocking, which must be fed into it through a GCLK generator and the associated mux channel. The source is also, initially, not activated – an appropriately accurate 32kHz clock – so that must be set up. Once the reference is fed into GCLK1 generator and into the DFLL mux (which will be ID 0), we need to activate and calibrate the DFLL source using coarse values stored on the hardware (as fuses, there position is written in the SAM library files).

Once calibrated, the DFLL source will be active and can be fed into the GCLK0  (which until now has been running on the OSC8M). After this, the MAINCLK of the device will be running at 48 MHz.

To validate the output, we publish the clock to the GCLK_IO related to GCLK0 (GCLK_IO[0]). Mind, this IO can only be a specific set of Ios, thus we need to figure out, where the signal will go. From the datasheet’s “I/O Multiplexing and Considerations” section, we can see that PA28 – if set to peripheral function type “H” – can receive this signal.

After this setup, the CPU will run at 48 MHz and we will be able to validate it by checking the PA28 pin with an oscilloscope.

### Reset OSC8M
As mentioned above, the OSC8M is already active and is divided by 8, running at 1 MHz. I decided to remove the pre-scaling before feeding it into the peripheral, albeit this isn’t necessary.

### Setting up the TC timer and its clocking
We will be using a TC timer to generate the delays.

Technically, we need to set up a source for our clock generator using the SYSCTRL.

Once that is done, we need to set the GCLK->GENCTRL register for our chosen GCLK, where the ID bits will pick the GCLK generator (one from the 8, we are using GCLK5). We also pick the source of the GCLK here (OSC8M). The GCLK generator must be enabled and pre-scaled (by 8 to have an output frequency of 1 MHz, GCLK->GENDIV register).

Lastly, we connect the GCLK to the clock channel we wish to run by adding values to the GCLK->CLLCTRL register. Here the ID bits define the clock channel. The channel must be enabled here.

Mind, we also need to remove the masking/enable the peripheral to allow it to be clocked, this is done in the “PM” register.

We finish by setting up the TC peripheral. We will be using COUNT16 and count upwards.  

### Delay function definition
This is self-explanatory. We do a blocking “while” loop until the TC counter reaches a designated value.

