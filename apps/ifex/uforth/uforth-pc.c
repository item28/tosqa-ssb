#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include "uforth.h"
#include "uforth-ext.h"

FILE *OUTFP;

#define CONFIG_IMAGE_FILE "uforth-config.img"
char modem_resp[80*2];

#define INITIAL_DICT {DICT_VERSION, MAX_DICT_CELLS, 0, 0, 1, {0}}

time_t uptime;

struct dict ramdict = INITIAL_DICT;
struct dict *dict = &ramdict;

uint8_t rxc(void) {
  return getchar();
}

void rxgetline(char* str) {
  fgets(str,128,stdin);
}

void txc(uint8_t c) {
  fputc(c, OUTFP);
  fflush(OUTFP);
}

void txs(char* s, int cnt) {
  fwrite(s,cnt,1,OUTFP);
  fflush(OUTFP);
}
#define txs0(s) txs(s,strlen(s))

static FILE *cfp;
char config_open_w(void) {
  cfp = fopen(CONFIG_IMAGE_FILE, "w+");
  return (cfp != NULL);
}

char config_open_r(void) {
  cfp = fopen(CONFIG_IMAGE_FILE, "r");
  return (cfp != NULL);
}

char config_write(char *src, uint16_t size) {
  fwrite((char*)src, size, 1, cfp);
  return 1;
}

char config_read(char *dest, uint16_t size) {
  fread((char*)dest, size, 1, cfp);
  return 1;
}
char config_close(void) {
  fclose(cfp);
  return 1;
}

void interpret_from(FILE *fp);


uforth_stat c_handle(void) {
  DCELL r2, r1 = dpop();
  char *str;
  FILE *fp;
  static char buf[80*2];

  switch(r1) {
  case UF_EMIT:			/* emit */
    txc(dpop()&0xff);
    break;
  case UF_KEY:			/* key */
    dpush((CELL)rxc());
    break;
  case UF_TYPE:			/* type */
    r1 = dpop();
    r2 = dpop();
    if (r2 >= RAM_START_IDX) {
      str = (char*)&uforth_ram[r2-RAM_START_IDX];
    }  else {
      str = (char*)&(uforth_dict[r2]);
    }
    txs(str,r1);
    break;
  case UF_SAVE_IMAGE:			/* save image */
    {
      uforth_next_word();
      strncpy(buf, uforth_iram->currword, uforth_iram->currwordlen);
      buf[uforth_iram->currwordlen] = '\0';
      printf("Saving dictionary into %s\n", buf);
      fp = fopen(buf, "w+");
      fwrite(dict, (dict_here())*sizeof(CELL) ,1,fp);
      fclose(fp);
    }
    break;
  case UF_INCLUDE:			/* include */
    {
      uforth_next_word();
      strncpy(buf,uforth_iram->currword, uforth_iram->currwordlen);
      buf[uforth_iram->currwordlen] = '\0';
      printf("   Loading %s\n",buf);
      fp = fopen(buf, "r");
      if (fp != NULL) {
	interpret_from(fp);
	fclose(fp);
      } else {
	printf("File not found: %s\n", buf);
      }
    }  
    break;

  default:
    return c_ext_handle_cmds(r1);
  }
  return OK;
}


static char line[128];
void interpret_from(FILE *fp) {
  int stat;
  int16_t lineno = 0;
  while (!feof(fp)) {
    ++lineno;
    if (fp == stdin) txs0(" ok\r\n");
    if (fp == stdin)
      rxgetline(line);
    else
      if (fgets(line,128,fp) == NULL) break;
    if (line[0] == '\n' || line[0] == '\0') continue;
    stat = uforth_interpret(line);
    switch(stat) {
    case E_NOT_A_WORD:
      fprintf(stdout," line: %d: ", lineno);
      txs0("Huh? >>> ");
      txs(uforth_iram->currword,uforth_iram->currwordlen);
      txs0(" <<< ");
      txs0(uforth_iram->currword + uforth_iram->currwordlen);
      txs0("\r\n");
      break;
    case E_ABORT:
      txs0("Abort!:<"); txs0(line); txs0(">\n");
      break;
    case E_STACK_UNDERFLOW:
      txs0("Stack underflow!\n");
      break;
    case E_DSTACK_OVERFLOW:
      txs0("Stack overflow!\n");
      break;
    case E_RSTACK_OVERFLOW:
      txs0("Return Stack overflow!\n");
      break;
    case OK:
      break;
    default:
      txs0("Ugh\n");
      break;
    }
  }
}

#include <sys/types.h>
#include <sys/stat.h>


int main(int argc, char* argv[]) {
  uptime = time(0);
  uforth_init();

  OUTFP = stdout;
#ifdef BOOTSTRAP
  {
    FILE *fp;
    printf("BOOTSTRAP Version\n");
    printf("   Loading init.f\n");
    fp = fopen("init.f", "r");
    if (fp != NULL) 
      interpret_from(fp);
    fclose(fp);
  }
#endif

  if (argc > 1) {
    uforth_interpret(argv[1]);
    return 0;
  }


  strcpy(modem_resp,"ERROR\r\n");

  c_ext_init();
  uforth_interpret("init");
  interpret_from(stdin);

  return 0;
}

