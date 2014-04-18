# include these rules to sorta/kinda use the LPCLink 1 & 2 as JTAG programmers
# requires an installed LPCXpresso IDE, paths below configured for Mac OSX

# convenience target, to find out how large the compiled code is
size: build/$(PROJECT).elf
	arm-none-eabi-size build/$(PROJECT).elf
	
# paths and settings used for uploading
LPCX = /Applications/lpcxpresso_7.1.1_125//lpcxpresso/bin
LPCX_PKG ?= LPC11C24/301
LPCX_ARGS = -wire=winUSB -vendor=NXP -p$(LPCX_PKG) -g

# upload commands, using utility code and data from the LPCXpresso IDE
dfu:
	$(LPCX)/dfu-util -d 0x471:0xdf55 -c 0 -t 2048 -R -D $(LPCX)/LPCXpressoWIN.enc
isp1: build/$(PROJECT).bin
	$(LPCX)/crt_emu_lpc11_13_nxp -flash-load-exec build/$(PROJECT).bin $(LPCX_ARGS)
isp2: build/$(PROJECT).bin
	$(LPCX)/crt_emu_cm_redlink -flash-load-exec build/$(PROJECT).bin $(LPCX_ARGS)
