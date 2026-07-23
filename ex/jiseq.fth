:ar son:push son:begin #0 $s son:opa son:end ;
:aa son:begin $s son:opb son:end ;
:ra son:begin #1 $s son:opa son:end son:pop son:rw-get ;
:at son:rw-set son:begin #2 $a son:opa son:end ;
#55 'base var-n
:ji #8 shift-left swap #18 shift-left or @base or ;
:alu son:push son:begin #2 $i son:opa son:end ;
:synth
phasor #0.200 cf #0.100 cf warp
#0.800 cf #0.800 cf #1.000 cf bez
#2000 c lpf #0.500 cf mul ;

son:patch-setup

#5 ar #2 at
#1 #1 ji aa #9 #8 ji aa #5 #4 ji aa #4 #3 ji aa
#3 #2 ji aa #5 #3 ji aa #15 #8 ji aa #2 #1 ji aa ra
'scl var-n

#2 ar #0 at
#0 aa #1 aa #0 aa #1 aa #0 aa #4 aa #1 aa
#2 aa #1 aa #0 aa #1 aa #0 aa #6 aa #1 aa
#7 aa #6 aa #5 aa #4 aa
ra 'notes var-n

#2 ar #0 at
#0 aa #1 aa #3 aa #2 aa
#0 aa #6 aa #5 aa #4 aa
ra 'bass var-n

#100 c clock
bdup #1 c #4 c rephasor metro

@notes iter @scl alu sequence

#0.001 cf smoother synth #300 c hpf

bswap #9 c #2 c rephasor metro
@bass iter @scl alu sequence
#1 c #2 c div mul
#0.030 cf smoother synth #500 c lpf add

bdup #200 c hpf
bdup #0.970 cf #10000 c bigverb bdrop #0.050 cf mul add

sink #0 tout
#0 topen #40 render #0 tclose
son:ctx-destroy
bye
