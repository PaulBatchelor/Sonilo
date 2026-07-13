:crv:tri #8 ;
:crv:smo #9 ;
:crv:lin #0 ;
:crv:qdu #7 ;
:vx #16 shift-left or swap #8 shift-left or arr-append ;
:iter son:iter ;
:ibnew son:begin #1 $i son:opa son:end ;
:curvi #65536 #5226 or ugen ;

son:patch-setup

ibnew son:pop son:rw-get 'ib var-n

#60 const clock
#5 #4 arr
#1 #1 crv:lin vx #1 #1 crv:smo vx
#2 #1 crv:lin vx #2 #1 crv:qdu vx iter
@ib son:push rephasori
crv:tri const curve
@ib son:push curvi
#12 const mul #60 const add mtof saw #300 const lpf
#0.800 constf mul

sink #0 tout

#0 topen
#10 render
#0 tclose

son:ctx-destroy
bye
