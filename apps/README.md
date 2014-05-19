Brief description of the applications in this area:

* **blinker** - alternating blink of the ok/bad LEDs on the SSB
* **bootsend** - upload code to an LPC11C24 in ROM-based CAN boot mode
* **canboot** - boot loader over CAN, using Tosqa conventions
* **canrecv** - copy received CAN data to motor pins
* **cansend** - periodic send of three ADC readouts
* **canuid** - send out the LPC11C24's unique device id
* **digipot** - move the digital vref adjust through its entire range
* **disco051** - test application for the STM32F051 Discovery board
* **disco303** - test application for the STM32F303 Discovery board
* **disco407** - CAN to serial USB bridge using the STM32F407 Discovery board
* **disco429** - test application for the STM32F429 Discovery board
* **driver** - complete CAN and stepper driver for the SSB
* **fakeboot** - test re-flashing code and the secondary boot mechanism
* **ifex** - code for the STM32-P103 "Interface Explorer" board, see wiki
* **knobdial** - read out rotary switches and drive other stepper boards
* **minimal** - absolutely minimal blinker, no ChibiOS or other libs
* **netdemo** - test application for the AOAA board's LPC1769
* **nete407** - test application for the Olimex STM32-E407 board
* **nucleo1** - CAN to serial USB bridge for the STM32 Nucleo F103 board
* **olip103** - test application for the Olimex STM32-P103 board
* **peeklcd** - show last 60 CAN bus messages on LCD display (using uGFX)
* **pwmstep** - a first test to drive the stepper motor from PWM
* **rampstep** - drive the stepper motor from PWM with linear ramping
* **rotaries** - interface two rotary encoders to the CAN bus
* **satdemo** - test the OLED display on the AOAA satellite's LPC11C24
* **satmon** - report incoming CAN messages on the AOAA satellite display
* **stepcan** - send out lots of stepper commands on the CAN bus
* **stepper** - drive the SSB's stepper using software timers

The most important firmware applications in the above list are:

The **bootsend** app includes a compiled copy of _canboot_, which it can send
to an LPC11C24 powered-up in native ROM-based CAN boot mode (PIO0_3 low). This
NXP-defined protocol runs over CAN at 100 KHz and uses CANopen'ish commands.
This code is for standalone use as "Boot Installer" for new LPC11C24 boards.

The **canboot** app is intended to be installed once on each LPC11C24, and
causes it to request a node ID and look for firmware upgrades on every reset.
It uses Tosqa conventions: a CAN bus @ 500 KHz, with boot requests and replies
taking place on extended CAN addresses 0x1F123400 .. 0x1F1234FF. Since this is
a secondary boot loader, any firmware upgrades must have been compiled to start
from address 0x1000, i.e. 4 KB higher than normal. All interrupt vectors are
_re-mapped_ to their corresponding vectors, i.e. also 4 KB higher than usual.

The **driver** app is the actual code for the Single Stepper Board. It listens
for commands coming in over the CAN bus, and drives the attached stepper motor.

The "`disco*`", "`net*`", "`oli*`", and "`sat*`" apps are for various boards
from Embedded Artists, Olimex, and STMicro.
