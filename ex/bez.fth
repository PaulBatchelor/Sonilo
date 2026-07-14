son:patch-setup

#50 c mtof phasor
#1.000 cf
#0.500 cf
#0.300 cf
#2 c clock #8 c curve #9 c curve
#0.700 cf mul add
bez

#50 #7 n:sub c mtof phasor
#0.000 cf
#0.900 cf
#0.800 cf
#3 c clock #8 c curve #9 c curve
#0.200 cf mul add
bez

#50 #9 n:add c mtof phasor
#1.000 cf
#0.900 cf
#1 c clock #8 c curve #9 c curve
#0.100 cf mul add
#0.990 cf
bez

add

add

#8000 c lpf

bdup bdup #0.900 cf #8000 c bigverb bdrop #0.100 cf mul add

#0.200 cf mul sink

#0 tout

#0 topen
#30 render
#0 tclose

son:ctx-destroy
bye
