#define RAM_DICT 1

struct dict *dict;

#include "uforth/uforth.c"
#include "uforth/uforth-ext.c"
#include "uforth/utils.c"

static bool_t getLine(char *line, unsigned size) {
  char *p = line;

  while (TRUE) {
    char c;

    // use timeouts so serio can be switched in mid-flight
    while (chnReadTimeout((BaseChannel*) serio, (uint8_t *)&c, 1, 1000) == 0)
      ;
    if (c == 4) {
      return TRUE;
    }
    if (c == 8 || c == 127) {
      if (p != line) {
        chSequentialStreamPut(serio, 8);
        chSequentialStreamPut(serio, 0x20);
        chSequentialStreamPut(serio, 8);
        p--;
      }
      continue;
    }
    if (c == '\r') {
      // chprintf(serio, "\r\n");
      *p = 0;
      return FALSE;
    }
    if (c < 0x20)
      c = ' ';
    if (p < line + size - 1) {
      chSequentialStreamPut(serio, c);
      *p++ = (char)c;
    }
  }
}

static void runForth(const uint8_t* romdict) {
  dict = (struct dict*) romdict;
  uforth_init();
  c_ext_init();
  static uint8_t buf[100];
  while (TRUE) {
    // chprintf(serio, "\r\n %d> ", uforth_uram->didx + 1);
    chprintf(serio, " OK\r\n");
    if (getLine((char*) buf, sizeof buf))
      break;
    chSequentialStreamPut(serio, ' ');
    int stat = uforth_interpret((char*) buf);
    switch(stat) {
    case E_NOT_A_WORD:
      chprintf(serio, "Huh? >>> ");
      chSequentialStreamWrite(serio, (uint8_t*) uforth_iram->currword, uforth_iram->currwordlen);
      chprintf(serio, " <<< %s\r\n", uforth_iram->currword + uforth_iram->currwordlen);
      break;
    case E_ABORT:
      chprintf(serio, "Abort!:<%s>\r\n", buf);
      break;
    case E_STACK_UNDERFLOW:
      chprintf(serio, "Stack underflow!\r\n");
      break;
    case E_DSTACK_OVERFLOW:
      chprintf(serio, "Stack overflow!\r\n");
      break;
    case E_RSTACK_OVERFLOW:
      chprintf(serio, "Return Stack overflow!\r\n");
      break;
    case OK:
      break;
    default:
      // txs0("Ugh\n");
      break;
    }
  }
}

uforth_stat c_handle(void) {
  DCELL r2, r1 = dpop();
  uint8_t *str;
  // static char buf[80*2];

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
      str = (uint8_t*)&uforth_ram[r2-RAM_START_IDX];
    }  else {
      str = (uint8_t*)&(uforth_dict[r2]);
    }
    chSequentialStreamWrite(serio, str, r1);
    break;
  case UF_MEM_AT:
    r1 = dpop();
    dpush(*(int*)r1);
    break;
  case UF_MEM_STORE:
    r1 = dpop();
    r2 = dpop();
    *(int*)r1 = r2;
    break;
  case UF_CMEM_AT:
    r1 = dpop();
    dpush(*(uint8_t*)r1);
    break;
  case UF_CMEM_STORE:
    r1 = dpop();
    r2 = dpop();
    *(uint8_t*)r1 = r2;
    break;
  case UF_CHMEM:
    cmd_mem();
    break;
  case UF_THREADS:
    cmd_threads();
    break;
  case UF_TREE:
    cmd_tree();
    break;
  default:
    return c_ext_handle_cmds(r1);
  }
  return OK;
}
