:dur swap #8 shift-left or arr-append ;
:iter son:iter ;
:ibnew son:begin #1 $i son:opa son:end ;

son:patch-setup

#130 const clock

bdup bdup bdup

#4 const #1 const rephasor metro
#3 #4 arr %10 %9 %8 %7 seq
#60 const add #0.030 constf smoother
mtof saw #200 const lpf #0.400 constf mul

bswap
#5 #4 arr
#1 #2 dur #1 #2 dur #1 #2 dur #1 #4 dur iter
ibnew
rephasori
metro
bdup
#0.005 constf #0.001 constf #0.010 constf env
bswap #3 #5 arr %0 %2 %3 %5 %7 seq
#60 #12 n:add const add mtof saw #500 const lpf #0.400 constf mul
mul

add

bdup bdup
#0.930 constf #8000 const bigverb bdrop #0.200 constf mul add

bswap

#5 #6 arr

#3 #4 dur
#3 #4 dur
#3 #4 dur
#3 #4 dur
#1 #2 dur
#1 #2 dur

iter ibnew
rephasori
metro
#4 #12 arr
%0 %7 %12 %7 %0 %7
%0 %7 %12 %7 %12 %14
seq #0.002 constf smoother
#60 #24 n:sub const add
mtof saw #300 const lpf #0.300 constf mul
add

#0.800 constf mul

sink #0 tout
#0 topen
#20 render

#0 tclose

son:ctx-destroy
bye
