uint32_t CANopen_SDOS_Exp_Read(uint16_t index, uint8_t subindex)
{
  (void)index;
  (void)subindex;
	return 0;  /* Return 0 for successs, SDO Abort code for error */
}

uint32_t CANopen_SDOS_Exp_Write(uint16_t index, uint8_t subindex, uint8_t *dat_ptr)
{
  (void)index;
  (void)subindex;
  (void)dat_ptr;
	return 0;  /* Return 0 for successs, SDO Abort code for error */
}

uint32_t CANopen_SDOS_Seg_Read (uint16_t index, uint8_t subindex, uint8_t openclose, uint8_t *length, uint8_t *data, uint8_t *last) {
  (void) index;
  (void) subindex;
  (void) openclose;
  (void) length;
  (void) data;
  (void) last;
	return 0;
}

uint32_t CANopen_SDOS_Seg_Write (uint16_t index, uint8_t subindex, uint8_t openclose, uint8_t length, uint8_t *data, uint8_t *fast_resp) {
  (void) index;
  (void) subindex;
  (void) openclose;
  (void) length;
  (void) data;
  (void) fast_resp;
	return 0;
}
