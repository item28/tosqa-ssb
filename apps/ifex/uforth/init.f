: include 24 cf ;

include core.f

include ext.f
include util.f


: forget ( -- )
    '& dup ['] forget > if (forget) then ;

: init ;
