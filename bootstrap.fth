#1024 dup !Blocks [ new save next ] times

new #1 set
0 (prelude)
1 '(sonilo-core) needs
save

new #3 set
0 (sonilo-core)
1 :son:ins #2164260864 ;
2 :son:send #8 io ;
3 :son:rw-get #9 io ;
4 :son:rw-set #10 io ;
5 :son:key #8 shift-left #10551296 n:add ;
6 :son:magic son:ins son:send ;
7 :son:sys swap #16 shift-left #204 #8 shift-left n:add n:add
8 son:ins n:add ; 
9 :son:rw-print #00 #03 son:sys son:send ;
10 :son:begin #00 #01 son:sys son:send ;
11 :son:end #00 #02 son:sys son:send ;
12 :son:msb #16 shift-right #2164326400 n:add ;
13 :son:lsb #65535 and #2164391936 n:add ;
14 :son:word dup son:msb son:send son:lsb son:send ;
15 :son:opa son:key son:ins n:add n:add son:send ;
save

new #4 set
0 (sonilo-core)
1 :son:opb #206 swap son:opa son:word ;
2 :setw son:begin #33 son:opb son:end ;
4 :cf
5 dup #0 lteq? n:abs #29 shift-left
6 swap n:abs dup #1000 n:mod #8192 n:mul #1000 n:div
7 swap #1000 n:div #13 shift-left or or
8 son:begin #99 son:opb son:end ;
9 :c #1000 n:mul cf ;
10 :son:ctx-destroy son:begin #1 #67 son:opa son:end ;
11 :son:ctx-create son:begin #0 #67 son:opa son:end ;
12 :son:go son:begin #0 #107 son:opa son:end ;
13 :son:push son:begin $w son:opb son:end ;
14 :son:pop son:begin #0 $w son:opa son:end ;
15 :son:cur son:begin #1 n:add #107 son:opa son:end ;
save

new #5 set
0 (sonilo-core)
1 :son:array-create son:push son:begin #0 $a son:opa son:end ;
2 :son:array-write (avn-)
3 (anv)
4 swap son:push son:push son:push
5 son:begin #1 $a son:opa son:end ;
6 :iter son:push son:begin #0 $i son:opa son:end ;
7 :last-ugen son:begin #0 #117 son:opa son:end ;
8 :tape #116 son:opa ;
9 :bind #16 shift-left or son:rw-set son:begin #2 tape son:end ;
10 :topen setw son:begin #0 tape son:end ;
11 :tclose setw son:begin #1 tape son:end ;
12 :render son:begin #114 son:opb son:end ;
13 :bdup son:begin #0 $p son:opa son:end ;
14 :bdrop son:begin #1 $p son:opa son:end ;
15 :bswap son:begin #2 $p son:opa son:end ;
save

new #6 set
0 (sonilo-core)
1 #0 'AA var-n
2 #0 'AI var-n
3 :arr-append @AA @AI rot son:push son:push son:push
4 son:begin #1 $a son:opa son:end @AI n:inc !AI ;
5 :sigil:% s:to-n arr-append ;
6 &sigil:% $% sigil:set
7 :arr #4 shift-left or son:array-create 
8 son:pop son:rw-get dup !AA #0 !AI ;
9 :tout last-ugen son:rw-get bind ;
10 :err son:begin #1 $! son:opa son:end son:rw-get 
11 #0 -eq? [ 'error s:put nl bye ] if ;
12 :son:patch-setup son:ctx-create #0 son:cur son:go ;
13 :s:to-short #0 swap [ $A n:sub swap #5 shift-left or ]
14 s:for-each #1 shift-left ;
save

new #7 set
0 (sonilo-core)
1 :ugen s:to-short son:begin $u son:opb son:end ;
3 :ibnew son:begin #1 $i son:opa son:end ;
4 :ugen-bits s:to-short swap #16 shift-left or
5 son:begin $u son:opb son:end ;
save

new #9 set
0 (sonilo-core) (ugens)
1 :metro 'MET ugen ;
2 :clock 'CLK ugen ;
3 :mtof 'MTF ugen ;
4 :saw 'SAW ugen ;
5 :lpf 'LPF ugen ;
6 :mul 'MUL ugen ;
7 :sequence 'SEQ ugen ;
8 :sink 'SNK ugen ;
9 :smoother 'SMO ugen ;
10 :add 'ADD ugen ;
11 :seq iter sequence ;
12 :bigverb 'BVR ugen ;
13 :env 'ENV ugen ;
14 :rephasor 'RPH ugen ;
15 :rephasori #1 'RPH ugen-bits ;
save

new #10 set
0 (sonilo-core) (ugens)
1 :curve 'CRV ugen ;
2 :crv:lin #00 ; :crv:stp #01 ; :crv:gl0 #02 ; :crv:gl1 #03 ;
3 :crv:gl2 #04 ; :crv:gl3 #05 ; :crv:qup #06 ; :crv:qdn #07 ;
4 :crv:tri #08 ; :crv:smo #09 ; 
5 :curvi #1 'CRV ugen-bits ;
6 :phasor 'PHS ugen ;
7 :bez 'BEZ ugen ;
8 :scale 'SCL ugen ;
9 :warp 'WRP ugen ;
10 :terp 'TRP ugen ;
11 :div 'DIV ugen ;
save

bye
