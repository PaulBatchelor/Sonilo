:formant 'FMT ugen ;
:vib 'VIB ugen ;
:dcblk 'DCB ugen ;

son:patch-setup

#60 c mtof
#5.800 cf phasor crv:tri c curve #150 c 
#0.200 cf phasor crv:tri c curve #1.000 #0.300 cf cf scale mul vib
mul
phasor #0.100 cf #0.100 cf warp #1.000 cf #1.000 cf #1.000 cf bez

bdup
#650 c #80 c formant
bswap bdup
#1080 c #90 c formant #0.500 cf mul
bswap bdup
#2650 c #120 c formant #0.400 cf mul
bswap
#2900 c #130 c formant #0.200 cf mul
add add add
#8000 c lpf #8000 c lpf

#0.200 cf mul

bdup bdup
#0.700 cf #10000 c bigverb bdrop #0.200 cf mul dcblk add
sink #0 tout
#0 topen #10 render #0 tclose

son:ctx-destroy
bye
