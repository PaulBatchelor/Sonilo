:vib 'VIB ugen ;
:hold son:begin #3 $p son:opa son:end ;
:unhold son:begin #4 $p son:opa son:end ;
:bpop son:begin #5 $p son:opa son:end son:rw-get ;
:bpush son:rw-set son:begin #6 $p son:opa son:end ;

son:patch-setup

#0.200 cf phasor crv:tri c curve hold bpop
'lfo var-n
#60 c mtof #5.500 cf phasor
#1 c #1 c #1 c bez
@lfo bpush
mul
#50 c vib mul
phasor
@lfo bpush #0.300 cf #0.100 cf scale
#0.010 cf warp #1 c #1 c #1 c bez
#1000 c lpf #0.500 cf mul
@lfo bpush unhold
sink #0 tout
#0 topen #10 render #0 tclose
son:ctx-destroy
bye
