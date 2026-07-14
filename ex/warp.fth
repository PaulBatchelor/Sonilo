son:patch-setup

#53 #12 n:sub const mtof phasor
#0.200 constf phasor
crv:tri const curve crv:smo const curve
#0.500 constf #0.100 constf scale
#0.500 constf warp

#1.000 constf
#6.000 constf phasor crv:tri const curve
#0.800 constf #1.000 constf scale
#0.600 constf bez

#8000 const lpf

#0.500 constf mul

sink #0 tout

#0 topen #11 render #0 tclose

son:ctx-destroy
bye
