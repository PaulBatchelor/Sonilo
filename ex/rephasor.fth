:mksaw const mtof saw #300 const lpf #0.400 constf mul ;

:mkenv swap const const rephasor metro
#0.005 constf #0.001 constf #0.010 constf env mul ;

son:patch-setup

#50 const clock

bdup bdup bdup
#62 mksaw bswap #1 #1 mkenv

bswap
#69 mksaw bswap #1 #3 mkenv
add

bswap
#74 mksaw bswap #1 #5 mkenv
add

bswap
#64 mksaw bswap #3 #2 mkenv
add

bdup

#78 const mtof saw #300 const lpf #0.200 constf mul add

bdup
#0.970 constf #8000 const bigverb bdrop #0.200 constf mul
bswap #0.900 constf mul
add

sink #0 tout #0 topen #10 render #0 tclose

son:ctx-destroy bye
