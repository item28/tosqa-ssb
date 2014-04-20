USE_OPT = -Os -fomit-frame-pointer
USE_LINK_GC = yes
USE_THUMB = yes
USE_VERBOSE_COMPILE = no

PROJECT = min

CHIBIOS = ../../../../ChibiOS/ChibiOS-RT

PORTLD  = $(CHIBIOS)/os/ports/GCC/ARMCMx/LPC11xx/ld

LDSCRIPT ?= $(TOSQA_COMMON)/LPC11C24.ld

CSRC = $(CHIBIOS)/os/ports/GCC/ARMCMx/crt0.c \
       $(CHIBIOS)/os/ports/GCC/ARMCMx/LPC11xx/vectors.c \
	   $(SRC_C)

INCDIR = $(CHIBIOS)/os/ports/common/ARMCMx/CMSIS/include \
         $(CHIBIOS)/os/ports/common/ARMCMx \
         $(CHIBIOS)/os/ports/GCC/ARMCMx \
         $(CHIBIOS)/os/ports/GCC/ARMCMx/LPC11xx \
		 $(CHIBIOS)/os/kernel/include \
		 $(CHIBIOS)/os/hal/platforms/LPC11xx \
		 $(TOSQA_COMMON)

MCU  = cortex-m0
TRGT = arm-none-eabi-
CC   = $(TRGT)gcc
CPPC = $(TRGT)g++
LD   = $(TRGT)gcc
CP   = $(TRGT)objcopy
AS   = $(TRGT)gcc -x assembler-with-cpp
OD   = $(TRGT)objdump
HEX  = $(CP) -O ihex
BIN  = $(CP) -O binary

TOPT = -mthumb -DTHUMB
CWARN = -Wall -Wextra -Wstrict-prototypes

DDEFS = -D__NEWLIB__

include $(CHIBIOS)/os/ports/GCC/ARMCMx/rules.mk
