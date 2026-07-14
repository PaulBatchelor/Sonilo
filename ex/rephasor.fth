:mksaw c mtof saw #300 c lpf #0.400 cf mul ;

:mkenv swap c c rephasor metro
#0.005 cf #0.001 cf #0.010 cf env mul ;

son:patch-setup

#50 c clock

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

#78 c mtof saw #300 c lpf #0.200 cf mul add

bdup
#0.970 cf #8000 c bigverb bdrop #0.200 cf mul
bswap #0.900 cf mul
add

sink #0 tout #0 topen #10 render #0 tclose

son:ctx-destroy bye
