{ Automatically generated from uforth-ext.h. Do not EDIT!!! }

: emit ( c -- )   1 cf ;
: key ( -- c )   2 cf ;
: type ( addr count -- )   3 cf ;
: n>str ( n -- addr )   4 cf ;
: save-image ( -- )   5 cf ;
: interp ( addr -- )   6 cf ;
: substr ( addr start len -- addr )   7 cf ;
: (forget) ( -- )   8 cf ;
: mem@ ( addr -- )   9 cf ;
: mem! ( val addr -- )   10 cf ;
: cmem@ ( addr -- )   11 cf ;
: cmem! ( val addr -- )   12 cf ;
: include ( <filename> -- )   24 cf ;
: chmem ( -- )   30 cf ;
: threads ( -- )   31 cf ;
: tree ( -- )   32 cf ;
