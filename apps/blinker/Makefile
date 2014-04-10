TOSQA_COMMON = ../../common

SRC_CPP = main.cpp

# ISP = crt_emu_lpc11_13_nxp  # use this with an LPCLink 1
ISP = crt_emu_cm_redlink  # use this with an LPCLink 2

# paths and settings used for uploading
LPCX = /Applications/lpcxpresso_7.0.0_92//lpcxpresso/bin
LINK_ARGS = -wire=winUSB -vendor=NXP -pLPC11C24/301 -g

# upload commands, using utility code and data from the LPCXpresso IDE
isp: build/firmware.bin
	$(LPCX)/$(ISP) -flash-load-exec build/firmware.bin $(LINK_ARGS)
dfu:
	$(LPCX)/dfu-util -d 0x471:0xdf55 -c 0 -t 2048 -R -D $(LPCX)/LPCXpressoWIN.enc

include $(TOSQA_COMMON)/rules.mk
