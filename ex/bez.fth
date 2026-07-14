:bez 'BEZ ugens ;
:phasor 'PHS ugens ;
son:patch-setup

#50 const mtof phasor
#1.000 constf
#0.500 constf
#0.300 constf
#2 const clock #8 const curve #9 const curve
#0.700 constf mul add
bez

#50 #7 n:sub const mtof phasor
#0.000 constf
#0.900 constf
#0.800 constf
#3 const clock #8 const curve #9 const curve
#0.200 constf mul add
bez

#50 #9 n:add const mtof phasor
#1.000 constf
#0.900 constf
#1 const clock #8 const curve #9 const curve
#0.100 constf mul add
#0.990 constf
bez

add

add

#8000 const lpf

bdup bdup #0.900 constf #8000 const bigverb bdrop #0.100 constf mul add

#0.200 constf mul sink

#0 tout

#0 topen
#30 render
#0 tclose

son:ctx-destroy
bye
