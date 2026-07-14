son:ctx-create #0 son:cur son:go

#43 c clock metro
#3 #8 arr %0 %3 %7 %10 %9 %5 %3 %2 seq
#63 c add #0.100 cf smoother mtof
saw

#600 c lpf #0.300 cf mul

#63 #12 n:sub c mtof saw
#63 #12 n:sub #7 n:add c mtof saw add
#0.200 cf mul
#500 c lpf #0.300 cf mul
add

bdup err bdup err
#0.920 cf err
#4000 c bigverb err
bdrop #0.300 cf mul err
add
sink err #0 tout
#0 topen #13 render #0 tclose
son:ctx-destroy bye
