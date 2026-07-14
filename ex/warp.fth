son:patch-setup

#53 #12 n:sub c mtof phasor
#0.200 cf phasor
crv:tri c curve crv:smo c curve
#0.500 cf #0.100 cf scale
#0.500 cf warp

#1.000 cf
#6.000 cf phasor crv:tri c curve
#0.800 cf #1.000 cf scale
#0.600 cf bez

#8000 c lpf

#0.500 cf mul

sink #0 tout

#0 topen #11 render #0 tclose

son:ctx-destroy
bye
