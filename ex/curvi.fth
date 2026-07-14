:vx #16 shift-left or swap #8 shift-left or arr-append ;
son:patch-setup

ibnew son:pop son:rw-get 'ib var-n

#60 c clock
#5 #4 arr
#1 #1 crv:lin vx #1 #1 crv:smo vx
#2 #1 crv:lin vx #2 #1 crv:qdn vx iter
@ib son:push rephasori
crv:tri c curve
@ib son:push curvi
#12 c mul #60 c add mtof saw #300 c lpf
#0.800 cf mul

sink #0 tout

#0 topen
#10 render
#0 tclose

son:ctx-destroy
bye
