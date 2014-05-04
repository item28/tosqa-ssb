\  uForth - A tiny ROMable 16-bit FORTH-like scripting language
\          for microcontrollers.
\         http://maplefish.com/todd/uforth.html
\         Dictionary Version 0.99
\
\  License for uForth 0.1 and later versions
\
\  Copyright © 2009 Todd Coram, todd@maplefish.com, USA.
\
\  Permission is hereby granted, free of charge, to any person obtaining
\  a copy of this software and associated documentation files (the
\  "Software"), to deal in the Software without restriction, including
\  without limitation the rights to use, copy, modify, merge, publish,
\  distribute, sublicense, and/or sell copies of the Software, and to
\  permit persons to whom the Software is furnished to do so, subject to
\  the following conditions:
\  
\  The above copyright notice and this permission notice shall be
\  included in all copies or substantial portions of the Software.
\  
\  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
\  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
\  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
\  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
\  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
\  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
\  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.



\ exit if stack value is < 1
\
: if>0 ( n -- ) 1 < if r> drop then ;


\ Allocate a character buffer 
\
: c-allot ( n -- ) CELL / allot ;

\ Counts of dictionary stored strings and RAM strings
\
: count ( addr -- addr cnt ) dup @ swap 1+  swap ;

: hex 16 base ! ;
: decimal 10  base ! ;
: binary 2 base ! ;

: min ( a b -- a|b) over over > if swap drop else drop then ;
: max ( a b -- a|b) over over < if swap drop else drop then ;

: cr 13 emit 10 emit ;
: space 32 emit ;

: char ( <char> -- c)
    32 word 1+ @ 255 and ;

: [char]
    [compile] lit
    32 word 1+ @ ,
; immediate

: s"   compiling?
    if 
        postpone ," ['] count , 
    else
        34 word count
    then
; immediate

: c"   compiling?
    if 
        postpone ," 
    else
        34 word
    then
; immediate

: ."   compiling?
    if
        postpone ," ['] count , ['] type ,
    else
        34 word count type
    then
; immediate

: str! ( from to -- )
    dup >r
    swap count ( -- to from_c count ) swap rot ( --  from_c count to ) 1+
    swap ( -- from_c to_c count ) dup >r move
    r> r> ( -- to count ) ! ;
    

\ Print out top item on stack
\
: . ( n -- ) sidx 1+ if>0 n>str count type ;

: .s
    [char] < emit sidx 1+ . [char] > emit 32 emit
    sidx 1+ if>0  sidx 1+ 0 do  dsa i + @ . 32 emit loop ;

\ Interactive debugging help
\
: debug-pre cr ." debug: " ;
: debug  cr ."     stack -> [" .s ." ]" cr key drop ;
: debug" [compile] debug-pre
    postpone ," [compile] count [compile] type  [compile] debug ; immediate

\ if not true, print out a string and abort to top level
\
: assert" ( f -- )
    [compile] 0=
    postpone if
    postpone ," [compile] count [compile] type
    [compile] abort
    postpone then
; immediate

\ Version stuff. Show off how we handle strings.
\
: memory
    9 emit
    ." Dict: " here .  ."  cells (" here 2* . ."  bytes) used out of "
    maxdict . ."  cells."
    ."  (" here 100 * maxdict / . ." % used)." cr
    9 emit
    ." System RAM : " ram . ."  cells (" ram 4 * . ."  bytes)" cr
    9 emit
    ." Task Data Stack: " dslen . ."  cells. " 
    ." Task Return Stack: " rslen . ."  cells."  cr
    9 emit
    ." Task RAM: " uram-top@ .
    ."  cells (" uram-top@ 4 * . ."  bytes) used out of " uram-size .
    ."  cells" cr ;


\ A more verbose include (help track memory usage).
\
\ : include 24 cf memory cr ;


\ Forget everything after (and including) a word
\ Find the address of a word ('&), dup it, get the previous link, make it
\ the last word address (lwa) and set the forgotten word's address as new here
\
\ : forget '& dup @ lwa ! _here ! ;

\ From word definition addres ('&) return the code address.
\
: cfa  ( addr -- addr)
    1+ count 63 and dup 2 / swap 2 mod + + ;

\ From a word definition address ('&) print the name of the word.
\
: .word ( addr -- )
    1+ count 63 and type ;

\ Words
\
variable _cc                    \ keep track of # of characters on a line

: words ( -- )
    cr
    0 _cc !
    lwa @                               \ pointer to last word
    begin
        dup                             \ keep it on the stack 
        \ Words look like this: 
        \ [prevlink] [immediate/primitive name length] [name] [code] 
        \ name lengths are stored in the lower 6 bits -> 0x3F=63
        1+ count 63 and dup             \ get length of word's name
        _cc @ + 74 > if cr 0 _cc ! then \ 74 characters per line
        dup _cc +! type 32 emit 32 emit \ print word's name
        2 _cc +!                        \ add in spaces
        @                               \ get prev link from last word
        dup 0 =                        \ 0 link means no more words!
    until
    drop cr ;
