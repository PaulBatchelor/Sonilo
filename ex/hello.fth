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
:pw #00 #33 son:opa ;
:setw #33 son:opb ;

son:magic
son:begin
pw
#305414945 setw
pw
son:end

bye
