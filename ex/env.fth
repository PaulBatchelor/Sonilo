son:patch-setup

#115 #4 n:mul const clock metro

bdup #3 #8 arr %0 %2 %4 %7 %11 %12 %11 %7 seq
#57 const add
#0.003 constf smoother mtof saw #300 const lpf #0.300 constf mul

bswap

#0.005 constf #0.001 constf #0.030 constf env mul

sink #0 tout #0 topen #10 render #0 tclose

son:ctx-destroy bye
