#define RAM_DICT 1

struct dict *dict;

#include "uforth/uforth.c"
#include "uforth/uforth-ext.c"
#include "uforth/utils.c"

BaseSequentialStream* serio;

void runForth(BaseSequentialStream* chp, const uint8_t* romdict) {
  chprintf(chp, "UF: %d\r\n", romdict);
  dict = (struct dict*) romdict;
  serio = chp;
  uforth_init();
  c_ext_init();
  uforth_interpret("11 22 + . words memory");
  // uforth_interpret("words");
  // uforth_interpret("11 22 + .");
  // uforth_interpret("init");
  // uforth_interpret("words");
}

uforth_stat c_handle(void) {
  DCELL r2, r1 = dpop();
  char *str;
  static char buf[80*2];

  switch(r1) {
  case UF_EMIT:			/* emit */
    chSequentialStreamPut(serio, dpop());
    break;
  case UF_KEY:			/* key */
    dpush(chSequentialStreamGet(serio));
    break;
  case UF_TYPE:			/* type */
    r1 = dpop();
    r2 = dpop();
    if (r2 >= RAM_START_IDX) {
      str = (char*)&uforth_ram[r2-RAM_START_IDX];
    }  else {
      str = (char*)&(uforth_dict[r2]);
    }
    chSequentialStreamWrite(serio, str, r1);
    break;
  default:
    return c_ext_handle_cmds(r1);
  }
  return OK;
}
