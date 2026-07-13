:gl1 #3 ;
:gl0 #2 ;
:gl2 #4 ;
:gl3 #5 ;
:smo #9 ;
:qup #7 ;
:tri #8 ;
:vx #16 shift-left or swap #8 shift-left or
swap #24 shift-left or arr-append ;
:iter son:iter ;
:ibnew son:begin #1 $i son:opa son:end ;
:curvi #65536 #5226 or ugen ;
:terp #40030 ugen ;
:qn #1 #1 ;
:en #1 #2 ;
:qdot #3 #2 ;
:do #0 ;
:re #2 ;
:mi #4 ;
:fa #5 ;
:so #7 ;
:la #9 ;
:ti #11 ;

son:patch-setup

ibnew son:pop son:rw-get 'ib var-n

#3 const clock tri const curve smo const curve
#5 const mul #1 const add
#89 const mul clock

#5 #4 arr

do qn gl1 vx
re qn gl1 vx
mi qdot gl3 vx
so en gl1 vx

iter
@ib son:push rephasori

@ib son:push curvi

@ib son:push terp

#53 const add mtof saw #300 const lpf
#0.800 constf mul

sink #0 tout

#0 topen
#30 render
#0 tclose

son:ctx-destroy
bye
