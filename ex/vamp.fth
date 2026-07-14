:dur swap #8 shift-left or arr-append ;

son:patch-setup

#130 c clock

bdup bdup bdup

#4 c #1 c rephasor metro
#3 #4 arr %10 %9 %8 %7 seq
#60 c add #0.030 cf smoother
mtof saw #200 c lpf #0.400 cf mul

bswap
#5 #4 arr
#1 #2 dur #1 #2 dur #1 #2 dur #1 #4 dur iter
ibnew
rephasori
metro
bdup
#0.005 cf #0.001 cf #0.010 cf env
bswap #3 #5 arr %0 %2 %3 %5 %7 seq
#60 #12 n:add c add mtof saw #500 c lpf #0.400 cf mul
mul

add

bdup bdup
#0.930 cf #8000 c bigverb bdrop #0.200 cf mul add

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
seq #0.002 cf smoother
#60 #24 n:sub c add
mtof saw #300 c lpf #0.300 cf mul
add

#0.800 cf mul

sink #0 tout
#0 topen
#20 render

#0 tclose

son:ctx-destroy
bye
