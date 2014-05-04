/*
  uForth - A tiny ROMable 16/32-bit FORTH-like scripting language
          for microcontrollers.
          http://maplefish.com/todd/uforth.html
          Version 2.0

  License for uForth 0.1 and later versions

  Copyright © 2009, 2010 Todd Coram, todd@maplefish.com, USA.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:
  
  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "uforth-config.h"
#include "uforth.h"

CELL *uforth_dict;                      /* treat dict struct like array */
abort_t _uforth_abort_request;  /* for emergency aborts */

struct uforth_iram *uforth_iram;
struct uforth_uram *uforth_uram;

INLINE void dpush(const DCELL w) { 
  if (uforth_uram->didx == (uforth_uram->dsize+uforth_uram->rsize-1)) return;
  else uforth_uram->ds[++uforth_uram->didx] = w;
}
INLINE DCELL dpop(void) { return uforth_uram->ds[uforth_uram->didx--]; }
INLINE DCELL dpick(const DCELL n) { return uforth_uram->ds[uforth_uram->didx-n]; }
INLINE void rpush(const DCELL w) {
  if (uforth_uram->ridx == (uforth_uram->dsize)) return;
  else uforth_uram->ds[--uforth_uram->ridx] = w;
}
INLINE DCELL rpop(void) { return uforth_uram->ds[uforth_uram->ridx++]; } 
INLINE DCELL rpick(const DCELL n) {return uforth_uram->ds[uforth_uram->ridx+n]; }

INLINE uint32_t dpop32(void) { return dpop(); }
INLINE void dpush32(const uint32_t w2) { dpush(w2); }


/*
  Words must be under 64 characters in length
*/
#define WORD_LEN_BITS 0x3F  
#define IMMEDIATE_BIT (1<<7)
#define PRIM_BIT     (1<<6)


DCELL uforth_ram[TOTAL_RAM_CELLS];

enum { 
  LIT=1, DLIT, ABORT, DEF, IMMEDIATE, URAM_BASE_ADDR, PICK, RPICK,  SWAP, 
  HERE, INCR_HERE, ABS, IRAM_FETCH,
  ADD, SUB, MULT, DIV, AND, JMP, JMP_IF_ZERO, SKIP_IF_ZERO, EXIT,
  OR, XOR, LSHIFT, RSHIFT, EQ_ZERO, DROP,
  NEXT, CNEXT,  EXEC, LESS_THAN, MAKE_TASK, SELECT_TASK,
  INVERT, COMMA, DCOMMA, RPUSH, RPOP, FETCH, STORE,  COMMA_STRING,
  VAR_ALLOT, CALLC,   FIND, FIND_ADDR, STR_STORE,
  POSTPONE, _CREATE, MOVE, PARSE_NUM, PARSE_FNUM, LAST_PRIMITIVE
};

INLINE uint32_t abs32(int32_t v) {
  return (v < 0) ? v*-1 : v ;
}

#ifdef SUPPORT_FLOAT_FIXED
#include <math.h>
#endif
DCELL parse_num(char *s, uint8_t base) {
  (void)base;
#ifdef SUPPORT_FLOAT_FIXED
  double f;
  char *p = s;
  while (*p != '\0' && *p != ' ' && *p != '.') ++p;
  if (*p == '.') {              /* got a dot, must be floating */
    f = strtod(s,NULL);
    return (DCELL)FIXED_PT_MULT(f);
  }
#endif
  // Treat base 10 as 0 so we can handle 0xNN as well as decimal.
  //
  return strtol(s,NULL, uforth_uram->base == 10 ? 0 : uforth_uram->base);
}


/*
 Every entry in the dictionary consists of the following cells:
  [index of previous entry]  
  [flags, < 128 byte name byte count]
  [name [optional pad byte]... [ data ..]
*/
CELL find_word(char* s, uint8_t len, DCELL* addr, char *immediate, char *prim);

CELL defining_word = 0;         /* are we currently defining a word? */
void make_word(char *str, uint8_t str_len) {
  CELL my_head = dict_here();

  dict_append(dict->last_word_idx);
  dict_append(str_len);
  dict_append_string(str, str_len);
  dict_set_last_word(my_head);
#ifdef DEBUG_2
  printf("Making: %.*s (%d): %d\n", str_len, str, str_len, dict_here());
#endif
}

void make_immediate(void) {
  dict_write((dict->last_word_idx+1), uforth_dict[dict->last_word_idx+1]|IMMEDIATE_BIT);
}

char next_char(void) {
  if (*uforth_iram->inbufptr == 0)
    return *uforth_iram->inbufptr;
  else
    return *uforth_iram->inbufptr++;
}

char* uforth_next_word (void) {
  while (isspace(*uforth_iram->inbufptr)) ++uforth_iram->inbufptr;
  uforth_iram->currword = uforth_iram->inbufptr;
  while (*uforth_iram->inbufptr != 0 && !isspace(*uforth_iram->inbufptr)) ++uforth_iram->inbufptr;
  uforth_iram->currwordlen = uforth_iram->inbufptr - uforth_iram->currword;
  return uforth_iram->currword;
}

void uforth_abort(void) {
  if (uforth_iram->compiling) {
    dict_append(ABORT);
  }
  uforth_iram->compiling = 0;
  if (uforth_aborting()) { 
    DCELL r1 = find_word("catch-abort", strlen("catch-abort"), 0, 0, 0);
    if (r1 != 0) {
      char *badword = uforth_iram->currword;
      uint8_t badwordlen = uforth_iram->currwordlen;
      dpush(uforth_abort_reason());
      uforth_abort_clr();
      uforth_interpret("catch-abort");
      uforth_iram->currword = badword;
      uforth_iram->currwordlen = badwordlen;
    } 
  }
  uforth_abort_clr();
  uforth_uram->ridx = uforth_uram->rsize + uforth_uram->dsize;
  uforth_uram->didx = -1;
}

#ifdef BOOTSTRAP
void store_prim(char* str, CELL val) {
  make_word(str,strlen(str));
  dict_append(val);
  dict_append(EXIT);
  dict_write((dict->last_word_idx+1), uforth_dict[dict->last_word_idx+1]|PRIM_BIT);
}
#endif

typedef uforth_stat (*wfunct_t)(void);
/* Scratch variables for exec */
static DCELL r1, r2, r3, r4;
static char *str1, *str2;
static char b;
static CELL cmd;


CELL uforth_make_task (DCELL uram, 
                       CELL ds,CELL rs,CELL rams) {
  struct uforth_uram* u = (struct uforth_uram*)
    (uforth_ram + sizeof(struct uforth_iram)+(uram*sizeof(DCELL)));
  u->len = rams;
  u->base = 10;
  u->dsize = ds;
  u->rsize = rs;
  u->ridx = ds + rs;
  u->didx = -1;
  return 1;
}
static CELL curtask_idx;
INLINE void uforth_select_task (CELL uram) {
  uforth_uram = (struct uforth_uram*)
    (uforth_ram + sizeof(struct uforth_iram)+(uram*sizeof(DCELL)));
  curtask_idx = uram;
}

void uforth_init(void) {
  uforth_dict = (CELL*)dict;
  uforth_iram = (struct uforth_iram*) uforth_ram;
  uforth_iram->compiling = 0;
  uforth_iram->total_ram = TOTAL_RAM_CELLS;

  uforth_make_task(0,TASK0_DS_CELLS,TASK0_RS_CELLS,TASK0_URAM_CELLS);
  uforth_select_task(0);
  uforth_abort_clr();
  uforth_abort();

  uforth_uram->base = 10;

#ifdef BOOTSTRAP
  dict->here = DICT_HEADER_WORDS+1;
  /*
    Store our primitives into the dictionary.
  */
  store_prim("cf", CALLC);
  store_prim("uram", URAM_BASE_ADDR);
  store_prim("+iram@", IRAM_FETCH);
  store_prim("immediate", IMMEDIATE);
  store_prim("abort", ABORT);
  store_prim("pick", PICK);
  store_prim("rpick", RPICK);
  store_prim("swap", SWAP);
  store_prim("0skip?", SKIP_IF_ZERO);
  store_prim("drop", DROP);
  store_prim("jmp", JMP);
  store_prim("0jmp?", JMP_IF_ZERO);
  store_prim("exec", EXEC);
  store_prim(",", COMMA);
  store_prim("d,", DCOMMA);
  store_prim(">num", PARSE_NUM);
  store_prim(">fnum", PARSE_FNUM);
  store_prim("dummy,", INCR_HERE);
  store_prim("abs", ABS);
  store_prim("+", ADD);
  store_prim("-", SUB);
  store_prim("and", AND);
  store_prim("or", OR);
  store_prim("xor", XOR);
  store_prim("invert", INVERT);
  store_prim("lshift", LSHIFT);
  store_prim("rshift", RSHIFT);
  store_prim("*", MULT);
  store_prim("/", DIV);
  store_prim("0=", EQ_ZERO);
  store_prim("lit", LIT);
  store_prim("dlit", DLIT);
  store_prim(">r", RPUSH);
  store_prim("r>", RPOP);
  store_prim("@", FETCH);
  store_prim("!", STORE);
  store_prim(";", EXIT);  make_immediate();
  store_prim(":", DEF); 
  store_prim("_create", _CREATE); 
  store_prim("_allot1", VAR_ALLOT);

  store_prim("make-task", MAKE_TASK);
  store_prim("select-task", SELECT_TASK);
  store_prim("(find)", FIND);
  store_prim("(find&)", FIND_ADDR);
  store_prim(",\"", COMMA_STRING); make_immediate();
  store_prim("postpone", POSTPONE); make_immediate();
  store_prim("next-word", NEXT);
  store_prim("next-char", CNEXT);
  store_prim("str++", STR_STORE);
  store_prim("move", MOVE);
  store_prim("here", HERE);

  store_prim("<", LESS_THAN);

#endif
}


/* Return a counted string pointer
 */
char* uforth_count_str(CELL addr,CELL* new_addr) {
  char *str;
  if (addr >= RAM_START_IDX) {
    str =(char*)&uforth_ram[(addr-RAM_START_IDX)+1];
    *new_addr = uforth_ram[addr-RAM_START_IDX];
  } else {
    str =  (char*)&uforth_dict[addr+1];
    *new_addr = uforth_dict[addr];
  }
  return str;
}

uforth_stat exec(CELL wd_idx, char toplevelprim,uint8_t last_exec_rdix) {
  while(1) {
    if (wd_idx == 0) {
      uforth_abort_request(ABORT_ILLEGAL);
      uforth_abort();           /* bad instruction */
      return E_NOT_A_WORD;
    }
    cmd = uforth_dict[wd_idx++];
    switch (cmd) {
    case 0:
      uforth_abort_request(ABORT_ILLEGAL);
      uforth_abort();           /* bad instruction */
      return E_NOT_A_WORD;
    case ABORT:
      uforth_abort_request(ABORT_WORD);
      break;
    case IMMEDIATE:
      make_immediate();
      break;
    case IRAM_FETCH:
      r1 = dpop();
      dpush(((DCELL*)uforth_iram)[r1]);
      break;
    case URAM_BASE_ADDR:
      dpush((RAM_START_IDX + (sizeof(struct uforth_iram))) + curtask_idx);
      break;
    case SKIP_IF_ZERO:
      r1 = dpop(); r2 = dpop();
      if (r2 == 0) wd_idx += r1;
      break;
    case DROP:
      dpop();
      break;
    case JMP:
      wd_idx = dpop();
      break;
    case JMP_IF_ZERO:
      r1 = dpop(); r2 = dpop();
      if (r2 == 0) wd_idx = r1;
      break;
    case HERE:
      dpush(dict_here());
      break;
    case INCR_HERE:
      dict_incr_here(1);
      break;
    case LIT:  
      dpush(uforth_dict[wd_idx++]);
      break;
    case DLIT:  
      dpush((((uint32_t)uforth_dict[wd_idx])<<16) |
            (uint16_t)uforth_dict[wd_idx+1]); 
      wd_idx+=2;
      break;
    case LESS_THAN:
      r1 = dpop(); r2 = dpop();
      dpush(r2 < r1);
      break;
    case ABS:
      r1 = dpop(); 
      dpush(abs32(r1)); 
      break;
    case ADD:
      r1 = dpop(); r2 = dtop(); 
      dtop() = r1+r2;
      break;
    case SUB:
      r1 = dpop(); r2 = dtop(); 
      dtop() = r2-r1;
      break;
    case AND:
      r1 = dpop(); r2 = dtop();
      dtop() = r1&r2;
      break;
    case LSHIFT:
      r1 = dpop(); r2 = dtop();
      dtop() = r2<<r1;
      break;
    case RSHIFT:
      r1 = dpop(); r2 = dtop(); 
      dtop() = r2>>r1;
      break;
    case OR:
      r1 = dpop(); r2 = dtop(); 
      dtop() = r1|r2;
      break;
    case XOR:
      r1 = dpop(); r2 = dtop(); 
      dtop() = r1^r2;
      break;
    case INVERT:
      dtop() = ~dtop();
      break;
    case MULT:
      r1 = dpop(); r2 = dtop(); 
      dtop() = r1*r2;
      break;
    case DIV :
      r1 = dpop(); r2 = dtop(); 
      dtop() = r2/r1;
      break;
    case SWAP:
      r1 = dpop(); r2 = dpop(); 
      dpush(r1); dpush(r2);
      break;
    case RPICK:
      r1 = dpop();
      r2 = rpick(r1);
      dpush(r2);
      break;
    case PICK:
      r1 = dpop();
      r2 = dpick(r1);
      dpush(r2);
      break;
    case EQ_ZERO:
      dtop() = (dtop() == 0);
      break;
    case RPUSH:
      rpush(dpop());
      break;
    case RPOP:
      dpush(rpop());
      break;
    case FETCH:
      r1 = dpop();
      if (r1 >= RAM_START_IDX) {
        dpush(uforth_ram[r1-RAM_START_IDX]);
      } else {
        dpush(uforth_dict[r1]);
      }
      break;
    case STORE:
      r1 = dpop();
      r2 = dpop();
      if (r1 >= RAM_START_IDX) {
        uforth_ram[r1-RAM_START_IDX] = r2;
      } else {
        dict_write(r1,r2);
      }
      break;
    case EXEC:
      r1 = dpop();
      rpush(wd_idx);
      wd_idx = r1;
      break;
    case EXIT:
      if (uforth_uram->ridx > last_exec_rdix) return OK;
      wd_idx = rpop();
      break;
    case CNEXT:
      b = next_char();
      dpush(b);
      break;
    case STR_STORE:
      r1 = dpop();
      r2 = uforth_ram[(r1-RAM_START_IDX)];
      str1 =(char*)&uforth_ram[(r1-RAM_START_IDX)+1];
      str1+=r2;
      b = dpop();
      *str1 = b;
      uforth_ram[(r1-RAM_START_IDX)]++;
      break;
    case NEXT:
      str2 = PAD_STR;
      str1 = uforth_next_word();
      memcpy(str2,str1, uforth_iram->currwordlen);
      PAD_STRLEN = uforth_iram->currwordlen;            /* length */
      dpush(PAD_ADDR+RAM_START_IDX);
      break;
    case COMMA_STRING:
      if (uforth_iram->compiling > 0) {
        dict_append(LIT);
        dict_append(dict_here()+4); /* address of counted string */

        dict_append(LIT);
        rpush(dict_here());     /* address holding adress  */
        dict_incr_here(1);      /* place holder for jump address */
        dict_append(JMP);
      }
      rpush(dict_here());
      dict_incr_here(1);        /* place holder for count*/
      r1 = 0;
      b = next_char();          /* eat space */
      while (b != 0 && b!= '"') {
        r2 = 0;
        b = next_char();
        if (b == 0 || b == '"') break;
        r2 |= BYTEPACK_FIRST(b);
        ++r1;
        b = next_char();
        if (b != 0 && b != '"') {
          ++r1;
          r2 |= BYTEPACK_SECOND(b);
        }
        dict_append(r2);
      } 
      dict_write(rpop(),r1);
      if (uforth_iram->compiling > 0) {
        dict_write(rpop(),dict_here()); /* jump over string */
      }
      break;
    case CALLC:
      r1 = c_handle();
      if (r1 != OK) return (uforth_stat)r1;
      break;
    case VAR_ALLOT:
      dpush(VAR_ALLOT_1());
      break;
    case DEF:
      uforth_iram->compiling = 1;
    case _CREATE:
      dict_start_def();
      uforth_next_word();
      make_word(uforth_iram->currword,uforth_iram->currwordlen);
      if (cmd == _CREATE) {
        dict_end_def();
      } else {
        defining_word = dict_here();
      }
      break;
    case COMMA:
      dict_append(dpop());
      break;
    case DCOMMA:
      r1 = dpop();
      dict_append((uint32_t)r1>>16);
      dict_append(r1);
      break;
    case PARSE_NUM:
      r1 = dpop();
      str1=uforth_count_str((CELL)r1,(CELL*)&r1);
      str1[r1] = '\0';
      dpush(parse_num(str1,uforth_uram->base));
      break;
    case PARSE_FNUM:
      r1 = dpop();
      str1=uforth_count_str((CELL)r1,(CELL*)&r1);
      str1[r1] = '.';
      str1[r1+1] = '\0';
      dpush(parse_num(str1,uforth_uram->base));
      break;
    case FIND:
    case FIND_ADDR:
      r1 = dpop();
      str1=uforth_count_str((CELL)r1,(CELL*)&r1);
      r1 = find_word(str1, r1, &r2, 0, &b);
      if (r1 > 0) {
        if (b) r1 = uforth_dict[r1];
      }
      if (cmd == FIND) {
        dpush(r1);
      } else {
        dpush(r2);
      }
      break;
    case POSTPONE:
      str1 = uforth_next_word();
      r1 = find_word(str1, uforth_iram->currwordlen, 0, 0, &b);
      if (r1 == 0) {
        uforth_abort_request(ABORT_NAW);
        uforth_abort();
        return E_NOT_A_WORD;
      }
      if (b) {
        dict_append(uforth_dict[r1]);
      } else {
        dict_append(r1);
      }
      break;
    case MOVE:
      r1 = dpop();              /* count */
      rpush(r1);

      r1 = dpop();              /* dest */
      r2 = dpop();              /* source */

      if (r1 < RAM_START_IDX) {
        (void)rpop();
        uforth_abort_request(ABORT_ILLEGAL);
        uforth_abort();         /* can't cmove into dictionary */
        return E_ABORT;
      }
      str1 =(char*)&uforth_ram[(r1-RAM_START_IDX)];

      if (r2 > RAM_START_IDX) {
        str2 =(char*)&uforth_ram[(r2-RAM_START_IDX)];
      } else {
        str2 =  (char*)&uforth_dict[r2];
      }
      memcpy(str1, str2, rpop());
      break;
    case MAKE_TASK:
      r1 = dpop();
      r2 = dpop();
      r3 = dpop();
      r4 = dpop();
      uforth_make_task(r1-RAM_START_IDX, r2, r3, r4);
      dpush(r1-RAM_START_IDX);
      break;
    case SELECT_TASK:
      uforth_select_task(dpop());
      break;
    default:
      /* Execute user word by calling until we reach primitives */
      rpush(wd_idx);
      wd_idx = uforth_dict[wd_idx-1]; /* wd_idx-1 is current word */
      break;
    }
    if (uforth_aborting()) {
      uforth_abort();
      return E_ABORT;
    }
    if (toplevelprim) return OK;
  } /* while(1) */
}

CELL find_word(char* s, uint8_t slen, DCELL* addr, char *immediate, char *primitive) {
  CELL fidx = dict->last_word_idx;
  CELL prev = fidx;
  uint8_t wlen;

  while (fidx != 0) {
    if (addr != 0) *addr = prev ;
    prev = uforth_dict[fidx++];
    wlen = uforth_dict[fidx++]; 
    if (immediate) *immediate = (wlen & IMMEDIATE_BIT) ? 1 : 0;
    if (primitive) *primitive = (wlen & PRIM_BIT) ? 1 : 0;
    wlen &= WORD_LEN_BITS;
    if (wlen == slen && strncmp(s,(char*)(uforth_dict+fidx),wlen) == 0) {
      fidx += (wlen / BYTES_PER_CELL) + (wlen % BYTES_PER_CELL);
      return fidx;
    }
    fidx = prev;
  }
  return 0;
}


uforth_stat uforth_interpret(char *str) {
  uforth_stat stat;
  char *word;
  CELL wd_idx;
  char immediate = 0;
  char primitive = 0;

  uforth_iram->inbufptr = str;
  while(*(word = uforth_next_word()) != 0) {
    wd_idx = find_word(word,uforth_iram->currwordlen,0,&immediate,&primitive);
    switch (uforth_iram->compiling) {
    case 0:                     /* interpret mode */
      if (wd_idx == 0) {        /* number or trash */
        DCELL num = parse_num(word,uforth_uram->base);
        if (num == 0 && word[0] != '0') {
          uforth_abort_request(ABORT_NAW);
          uforth_abort();
          return E_NOT_A_WORD;
        }
        if (abs32(num) > (int32_t)MAX_CELL_NUM){
          dpush32(num);
        } else {
          dpush(num);
        }
      } else {
        stat = exec(wd_idx,primitive,uforth_uram->ridx-1);
        if (stat != OK) {
          uforth_abort();
          uforth_abort_clr();
          return stat;
        }
      }
      break;
    case 1:                     /* in the middle of a colon def */
      if (wd_idx == 0) {        /* number or trash */
        DCELL num = parse_num(word,uforth_uram->base);
        if (num == 0 && word[0] != '0') {
          uforth_abort_request(ABORT_NAW);
          uforth_abort();
          dict_end_def();
          return E_NOT_A_WORD;
        }
        /* OPTIMIZATION: Only DLIT big numbers */
        if (num < 0 || abs32(num) > (int32_t)MAX_CELL_NUM){
          dict_append(DLIT);
          dict_append(((uint32_t)num)>>16);
          dict_append(((uint16_t)num)&0xffff);
        } else {
          dict_append(LIT);
          dict_append(num);
        }
      } else if (word[0] == ';') { /* exit from a colon def */
        uforth_iram->compiling = 0;
        dict_append(EXIT);
        dict_end_def();
        defining_word = 0;
      } else if (immediate) {   /* run immediate word */
        stat = exec(wd_idx,primitive,uforth_uram->ridx-1);
        if (stat != OK) {
          uforth_abort_request(ABORT_ILLEGAL);
          uforth_abort();
          dict_end_def();
          return stat;
        }
      } else {                  /* just compile word */
        if (primitive) {
          /* OPTIMIZATION: inline primitive */
          dict_append(uforth_dict[wd_idx]);
        } else {
          /* OPTIMIZATION: skip null definitions */
          if (uforth_dict[wd_idx] != EXIT) {
            if (wd_idx == defining_word) { 
              /* Natural recursion for such a small language is dangerous.
                 However, tail recursion is quite useful for getting rid
                 of BEGIN AGAIN/UNTIL/WHILE-REPEAT and DO LOOP in some
                 situations. We don't check to see if this is truly a
                 tail call, but we treat it as such.
              */
              dict_append(LIT);
              dict_append(defining_word);
              dict_append(JMP);
            } else {
              dict_append(wd_idx);
            }
          }
        }
      }
      break;
    }
  }
  return OK;
}

