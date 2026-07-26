:hold son:begin #3 $p son:opa son:end ;
:unhold son:begin #4 $p son:opa son:end ;
:bpop son:begin #5 $p son:opa son:end son:rw-get ;
:bpush son:rw-set son:begin #6 $p son:opa son:end ;

:ar son:push son:begin #0 $s son:opa son:end ;
:aa son:begin $s son:opb son:end ;
:ra son:begin #1 $s son:opa son:end son:pop son:rw-get ;
:at son:rw-set son:begin #2 $a son:opa son:end ;
:hardsync 'HSY ugen ;
:powerwave 'PWV ugen ;

son:patch-setup

#105 c clock hold bpop 'Clk var-n

@Clk bpush #1 c #4 c rephasor metro
bdup
#3 ar #0 at
#0 aa #2 aa #0 aa #2 aa
#3 aa #2 aa #0 aa #2 aa
#0 aa #2 aa #0 aa #2 aa
#3 aa #2 aa #0 aa #2 aa
ra seq
bswap
#0 ar #0 at
#1 aa #0 aa #0 aa #1 aa
#0 aa #0 aa #1 aa #0 aa
#0 aa #1 aa #0 aa #0 aa
#1 aa #0 aa #1 aa #1 aa
#1 aa #0 aa #0 aa #1 aa
#0 aa #0 aa #1 aa #0 aa
#0 aa #1 aa #0 aa #0 aa
#1 aa #1 aa #0 aa #1 aa
ra seq #12 c mul add

@Clk bpush #4 c #1 c rephasor metro
#3 ar #0 at #33 aa #36 aa ra seq

add #0.001 cf smoother 

mtof hold bpop 'Frq var-n

@Frq bpush
@Clk bpush #16 c #1 c rephasor crv:tri c curve
#1.000 cf #14.000 cf scale mul
@Frq bpush phasor

hardsync

#0.100 cf #0.500 cf warp
powerwave #0.300 cf mul
#500 c hpf 
@Frq bpush saw #0.300 cf mul add

#2000 c lpf

sink #0 tout

#0 topen
#20 render
#0 tclose

son:ctx-destroy
bye
