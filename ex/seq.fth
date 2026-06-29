:son:ins #2164260864 ;
:son:send #8 io ;
:son:rw-get #9 io ;
:son:rw-set #10 io ;
:son:key #8 shift-left #10551296 n:add ;
:son:magic son:ins son:send ;
:son:sys swap #16 shift-left #204 #8 shift-left n:add n:add
son:ins n:add ; 
:son:rw-print #00 #03 son:sys son:send ;
:son:begin #00 #01 son:sys son:send ;
:son:end #00 #02 son:sys son:send ;
:son:msb #16 shift-right #2164326400 n:add ;
:son:lsb #65535 and #2164391936 n:add ;
:son:word dup son:msb son:send son:lsb son:send ;
:son:opa son:key son:ins n:add n:add son:send ;
:son:opb #206 swap son:opa son:word ;
:pw son:begin #00 #33 son:opa son:end ;
:setw son:begin #33 son:opb son:end ;
:ugen son:begin #117 son:opb son:end ;
:int #13 shift-left ;
:son:neg #1 shift-left #29 ;
:half #4096 ;
:quart #2048 ;
:eighth #1024 ;
:metro #24870 ugen ;
:const son:begin #99 son:opb son:end ;
:clock #4820 ugen ;
:mtof #25802 ugen ;
:saw #36908 ugen ;
:lpf #23498 ugen ;
:mul #25878 ugen ;
:sink #37716 ugen ;
:last-ugen son:begin #0 #117 son:opa son:end ;
:tape #116 son:opa ;
:bind son:begin #2 tape son:end ;
:topen son:begin #0 tape son:end ;
:tclose son:begin #1 tape son:end ;
:render son:begin #114 son:opb son:end ;
:son:ctx-init son:begin #0 #67 son:opa son:end ;
:son:cur son:begin #1 n:add #107 son:opa son:end ;
:son:go son:begin #0 #107 son:opa son:end ;

son:ctx-init
#0 son:cur son:go
#62 int const mtof saw #200 int const lpf half const mul sink
#0 setw topen
#0 last-ugen son:rw-get #16 shift-left or setw bind
#11 render
#0 setw tclose
bye
