son:ctx-create #0 son:cur son:go

#43 const clock metro
#3 #8 arr %0 %3 %7 %10 %9 %5 %3 %2 seq
#63 const add #0.100 constf smoother mtof
saw

#600 const lpf #0.300 constf mul

#63 #12 n:sub const mtof saw
#63 #12 n:sub #7 n:add const mtof saw add
#0.200 constf mul
#500 const lpf #0.300 constf mul
add

bdup err bdup err
#0.920 constf err
#4000 const bigverb err
bdrop #0.300 constf mul err
add
sink err #0 tout
#0 topen #13 render #0 tclose
son:ctx-destroy bye
