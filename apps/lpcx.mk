# include these rules to sorta/kinda use the LPCLink 1 & 2 as JTAG programmers
# requires an installed LPCXpresso IDE, paths below configured for Mac OSX

# ISP = crt_emu_lpc11_13_nxp  # use this with an LPCLink 1
ISP = crt_emu_cm_redlink  # use this with an LPCLink 2

# paths and settings used for uploading
LPCX = /Applications/lpcxpresso_7.1.1_125//lpcxpresso/bin
LINK_ARGS = -wire=winUSB -vendor=NXP -pLPC11C24/301 -g

# upload commands, using utility code and data from the LPCXpresso IDE
isp: build/firmware.bin
	$(LPCX)/$(ISP) -flash-load-exec build/firmware.bin $(LINK_ARGS)
dfu:
	$(LPCX)/dfu-util -d 0x471:0xdf55 -c 0 -t 2048 -R -D $(LPCX)/LPCXpressoWIN.enc
