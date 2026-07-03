son:ctx-create #0 son:cur son:go

#45 const clock metro
#3 #8 arr %0 %3 %7 %10 %9 %5 %3 %2 seq
#62 const add #0.080 constf smoother mtof
saw

#200 const lpf #0.300 constf mul
bdup err bdup err
#0.900 constf err
#10000 const bigverb err
bdrop #0.300 constf mul err
add
sink err #0 tout
#0 topen #13 render #0 tclose
son:ctx-destroy bye
