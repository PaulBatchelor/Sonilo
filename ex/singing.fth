:gpe 'GPE ugen ;
:bpf 'BPF ugen ;
#56 'base var-n
:ji #8 shift-left swap #18 shift-left or @base or arr-append ;
:gl1 #3 ;
:gl0 #2 ;
:gl2 #4 ;
:gl3 #5 ;
:lin #0 ;
:smo #9 ;
:qup #7 ;
:tri #8 ;
:vx #16 shift-left or swap #8 shift-left or
swap #24 shift-left or arr-append ;
:qn #1 #1 ;
:hn #2 #1 ;
:en #1 #2 ;
:sn #1 #4 ;
:qdot #3 #2 ;
:edot #3 #4 ;
:do #0 ;
:re #1 ;
:mi #2 ;
:fa #3 ;
:so #4 ;
:la #5 ;
:ti #6 ;
:Do #7 ;
:arr-type son:rw-set son:begin #2 $a son:opa son:end ;
:arr-gv dup son:push #1 arr-type son:pop ;
:arr-ji dup son:push #2 arr-type son:pop ;
:terpx #1 'TRP ugen-bits ;
:balance 'BAL ugen ;

son:patch-setup

(make-scale)
#5 #8 arr arr-ji
#1 #1 ji #9 #8 ji #5 #4 ji #4 #3 ji
#3 #2 ji #5 #3 ji #15 #8 ji #2 #1 ji
'ji-scl var-n

(make-index)
#5 #15 arr arr-gv
do qn gl1 vx
re qn gl2 vx
mi qn gl1 vx
fa qn gl1 vx
so qn gl1 vx
la qn gl1 vx
ti qn gl3 vx
Do qn gl1 vx
ti qn gl1 vx
la qn gl1 vx
so qn gl1 vx
fa qn gl1 vx
mi edot gl1 vx
so sn gl3 vx
re qn gl3 vx
'ji-seq var-n

ibnew son:pop son:rw-get 'ib var-n

#22 c clock @ji-seq iter @ib son:push rephasori
@ib son:push curvi
@ji-scl son:push @ib son:push terpx

#5.000 cf phasor #1 #1 #1 c c c bez #20 c vib mul
phasor

bdup
#0.400 cf glot
bswap
#0.400 cf gpe #0.900 cf mul #0.010 cf add

#12345678 son:push noise
#2000 c #2000 c bpf
mul

#0.020 cf mul

add

bdup

bdup
#800 c #80 c formant
bswap bdup
#1150 c #90 c formant #0.630 cf mul
bswap bdup
#2800 c #120 c formant #0.200 cf mul
bswap bdup
#3500 c #130 c formant #0.100 cf mul
bswap 
#4960 c #140 c formant #0.012 cf mul
add add add add

balance

#0.400 cf mul #100 c hpf sink #1 tout

@base c mtof #0.500 cf mul saw
@base c mtof #3 c #4 c div mul saw add

#0.500 cf mul

@base c mtof #1.500 cf mul lpf
#0.200 cf mul
add

bdup #200 c hpf bdup
#0.950 cf #10000 c bigverb bdrop dcblk #0.200 cf mul
add

#0 sink tout

#1 topen #0 topen #60 render #0 tclose #1 tclose

son:ctx-destroy
bye
