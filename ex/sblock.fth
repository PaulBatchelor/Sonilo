:ar son:push son:begin #0 $s son:opa son:end son:pop ;
:aa son:begin $s son:opb son:end ;
:ra son:begin #1 $s son:opa son:end son:pop son:rw-get ;
:lfo cf phasor crv:tri c curve cf cf scale ;

#55 'base var-n

son:patch-setup
#117 c clock bdup
#1 c #4 c rephasor metro
#3 ar #7 aa #0 aa #7 aa #2 aa #0 aa
ra seq

bswap bdup #10 c #4 c rephasor metro
#3 ar
@base aa @base #2 n:add aa @base #4 n:add aa @base #2 n:add aa
@base aa @base #2 n:add aa @base #7 n:add aa @base #4 n:add aa
ra seq

bswap #1 c #4 c rephasor metro #3 ar
#0 aa #0 aa #0 aa #0 aa
#12 aa #0 aa #0 aa #12 aa
ra seq #0.003 cf smoother add

add #0.001 cf smoother
mtof phasor
#0.100 #0.500 #0.100 lfo #0.200 cf warp
#0.000 cf #1.000 cf
#0.900 cf bez

#1800 c lpf #0.500 cf mul
sink #0 tout
#0 topen #10 render #0 tclose
son:ctx-destroy
bye
