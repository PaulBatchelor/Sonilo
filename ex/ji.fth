#64 'base var-n
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
:iterlu son:push son:begin #2 $i son:opa son:end ;
:arr-type son:rw-set son:begin #2 $a son:opa son:end ;
:arr-gv dup son:push #1 arr-type son:pop ;
:arr-ji dup son:push #2 arr-type son:pop ;
:terpx #1 'TRP ugen-bits ;
:hpf 'HPF ugen ;

son:patch-setup
(1:1)
(9:8)
(5:4)
(4:3)
(3:2)
(5:3)
(15:8)

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
mi edot gl0 vx
so sn gl2 vx
re qn gl3 vx
'ji-seq var-n

ibnew son:pop son:rw-get 'ib var-n

#19 c clock @ji-seq iter @ib son:push rephasori

@ib son:push curvi
@ji-scl son:push @ib son:push terpx

bdup
saw
bswap #2 c mul saw #0.300 cf mul add #0.700 cf mul

#800 c lpf
#300 c hpf

@base #24 n:sub c mtof saw
@base #36 n:sub c mtof saw
add #0.800 cf mul
#400 c lpf
add
#0.350 cf mul

bdup
#400 c hpf
bdup #0.970 cf #9000 c bigverb bdrop #100 c hpf #0.200 cf mul
bswap #0.600 cf mul add

sink #0 tout

#0 topen #65 render #0 tclose

son:ctx-destroy
bye
