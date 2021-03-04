[-]
+++++ +++++ 2 test cases
+++++ +++++
++++
[
read A
space >[-] ptr=1
id1 >[-]
id2 >[-]
Data >[-] ptr=4
>,>,>,>,>, >,>,>,>,>, ptr=14

read _B and a new line
space >[-] ptr=15
id1 >[-]
id2 >[-]
Data >[-]
>,,>,>,>,>,>, ptr=24
max_offset=5 [-]++++++

for each offset from 0 to 5
result 31=0
>>>>> >>[-]<<<<< <<
[-
for digit = 0 to 4
ptr = 25
>[-]+++++
digit result=1 ptr=30
>>>>>[-]+<<<<<
[-
  tmp0=index a = digit plus offset
  >[-] ptr=26
  tmp1=0 >[-]<
  tmp0=digit
  y   <[>+>+<<-]
  tmp1>>[<<+>>-]

  tmp0 = digit plus offset = tmp0 plus offset

  temp1 [-]
  y<<<[>>x+>+<<<-]
  >>>[<<<+>>>-]

  char a at ptr 28
  char a = A(tmp0)

  <
  ptr = 26 = tmp0 = index into A

  ptr = 1 = space of A
  <<<<< <<<<< <<<<< <<<<< <<<<<

space [-]
index1 >[-]
index2 >[-]
Data >[-] ptr = 4

index1 = z = ptr 26
>>>>> >>>>>
>>>>> >>>>> >>
z[-
 space<<<<< <<<<< <<<<< <<<<< <<<<<+
 index1>+
 z >>>>> >>>>> >>>>> >>>>> >>>>
]
space <<<<< <<<<< <<<<< <<<<< <<<<<[-
  >>>>> >>>>> >>>>> >>>>> >>>>>z+
  <<<<< <<<<< <<<<< <<<<< <<<<<
]

index2 = z
>>>>> >>>>>
>>>>> >>>>>
>>>>>
z[-
 space<<<<< <<<<< <<<<< <<<<< <<<<< +
 index2>>+
 z >>>>> >>>>> >>>>> >>>>> >>>
]
space <<<<< <<<<< <<<<< <<<<< <<<<<[-
  >>>>> >>>>> >>>>> >>>>> >>>>>z+
  <<<<< <<<<< <<<<< <<<<< <<<<<
]

>[>>>[-<<<<+>>>>]<<[->+<]<[->+<]>-]
>>>[-<+<<+>>>]<<<[->>>+<<<]>
[[-<+>]>[-<+>]<<<<[->>>>+<<<<]>>-]<<
>>>
char a at ptr=28=space 4 plus 24
>>>>> >>>>> >>>>> >>>>> >>>>[-]
<<<<< <<<<< <<<<< <<<<< <<<<
data[-
  >>>>> >>>>> >>>>> >>>>> >>>>+
  <<<<< <<<<< <<<<< <<<<< <<<<
]
  >>>>> >>>>> >>>>> >>>>> >>>>

  ptr=28
  char a = A (digit plus offset)

  <<
  ptr=26 tmp0

  [-]   tmp0=0
  >[-]< tmp1=0
  tmp0=digit
  y   <[>+>+<<-]
  tmp1>>[<<+>>-]
  < ptr=26

  ptr = 15 = space of B
  <<<<< <<<<< <

space [-]
index1 >[-]
index2 >[-]
Data >[-] ptr = 18

index1 = z = ptr 26
>>>>> >>>
z[-
 space<<<<< <<<<< <+
 index1>+
 z >>>>> >>>>>
]
space <<<<< <<<<< <[-
  >>>>> >>>>> >z+
  <<<<< <<<<< <
]

index2 = z
>>>>> >>>>> >
z[-
 space<<<<< <<<<< < +
 index2>>+
 z >>>>> >>>>
]
space <<<<< <<<<< <[-
  >>>>> >>>>> >z+
  <<<<< <<<<< <
]

>[>>>[-<<<<+>>>>]<<[->+<]<[->+<]>-]
>>>[-<+<<+>>>]<<<[->>>+<<<]>
[[-<+>]>[-<+>]<<<<[->>>>+<<<<]>>-]<<
>>> ptr=18 Data of B
char b at ptr=29
>>>>> >>>>> >[-]
<<<<< <<<<< <
data[-
  >>>>> >>>>> >+
  <<<<< <<<<< <
]
  >>>>> >>>>> >

  char a = char b == char a
  x<[-y>-<x]+y>[x<-y>[-]]  ptr=29

  ptr 30 digit result = digit result and char a ptr 28

  ptr=26

  temp0<<<[-]
  temp1>[-]
  x>>>[temp1<<<+x>>>-]
  temp1<<<[
  temp1[-]
  y>[temp1<+temp0<+y>>-]
  temp0<<[y>>+temp0<<-]
  temp1>[>>>x+temp1<<<[-]]
  ]
  >>>
  <
  <<<<ptr=25
]< end for each digit position

result 31 = result 31 or result 30
>>>>[-]
>[-]
>
[<<->]<+[<]+>[-<->>+>[<-<<+>>]<[-<]]>
 >[-]
 <<<[>>>+<<<-]
 <<<<
]end for each offset
>>>>> >>
  +++++ +++++
  +++++ +++++
  +++++ +++++
  +++++ +++++
  +++++ +++
  .
>[-]
  +++++ +++++.
<
<<<<< <<
dec num test cases
<<<<< <<<<< <<<<< <<<<< <<<<-
]