hello() ; Hello World example
 ; This is a simple MUMPS routine
 write "Hello, World!",!
 write "Welcome to MUMPS",!
 quit

greet(name) ; Greet a person by name
 ; Parameters: name - person's name
 write "Hello, ",name,"!",!
 quit

count() ; Count from 1 to 10
 new i
 for i=1:1:10 do
 . write i," "
 write !
 quit

factorial(n) ; Calculate factorial
 ; Parameters: n - number
 ; Returns: n!
 new result,i
 set result=1
 for i=1:1:n do
 . set result=result*i
 quit result
