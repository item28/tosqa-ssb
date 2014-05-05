#ifndef UF_UFORTH_EXT_H
#define UF_UFORTH_EXT_H

#define UF_EMIT                 1       // emit ( c -- )
#define UF_KEY                  2       // key  ( -- c )
#define UF_TYPE                 3       // type ( addr count -- )
#define UF_NUM_TO_STR           4       // n>str ( n -- addr )
#define UF_SAVE_IMAGE           5       // save-image ( -- )
#define UF_INTERP               6       // interp ( addr -- )
#define UF_SUBSTR               7       // substr ( addr start len -- addr )
#define UF_FORGET               8       // (forget) ( -- )
#define UF_MEM_AT               9       // mem@ ( addr -- )
#define UF_MEM_STORE            10      // mem! ( val addr -- )
#define UF_CMEM_AT              11      // cmem@ ( addr -- )
#define UF_CMEM_STORE           12      // cmem! ( val addr -- )
#define UF_INCLUDE              24      // include ( <filename> -- )

#define UF_MEM                  30      // mem ( -- )
#define UF_THREADS              31      // threads ( -- )
#define UF_TREE                 32      // tree ( -- )
#endif  /* UF_UFORTH_EXT_H */
