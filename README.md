Fungewars
=========

[Corewars](http://www.corewars.org/) in [Funge](http://quadium.net/funge/spec98.html)

## Documentation

TODO

## User Interface

TODO

## Instruction List

Under Construction

char| stack                         | name       	|op
----|-------------------------------|---------------|----
spc |( - )							| nop			| 
!   |(a - !a)						| not			| 
"   |( - string)					| string 		| pushes cells until it hits another ", then goes back to normal mode
#   |( - )							| skip			| (x, y) += (dx, dy)
$   |(a - )							| drop			| 
%   |(a b - a%b)					| modulus		| 
&   |( - )							| 				| 
'   |( - get(x+dx, y+dy))			| fetch			| (x, y) += (dx, dy)
(   |( - )							| 				| 
)   |( - )							| 				| 
*   |(a b - a*b)					| multiply		| 
+   |(a b - a+b)					| add			| 
,   |( - )							| 				| 
-   |(a b - a-b)					| subtract		| 
.   |( - )							| 				| 
/   |(a b - a/b)					| divide		| 
0   |( - 0)							| push 0		| 
1   |( - 1)							| push 1		| 
2   |( - 2)							| push 2		| 
3   |( - 3)							| push 3		| 
4   |( - 4)							| push 4		| 
5   |( - 5)							| push 5		| 
6   |( - 6)							| push 6		| 
7   |( - 7)							| push 7		| 
8   |( - 8)							| push 8		| 
9   |( - 9)							| push 9		| 
:   |(a - a a)						| dup			| 
;   |( - )							| jump			| pretends everything is a nop until it hits another ;
<   |( - )							| go left		| (dx, dy) = (-1, 0)
=   |( - )							| 				| 
>   |( - )							| go right		| (dx, dy) = ( 1, 0)
?   |( - )							| go random		| (dx, dy) = (1,0)|(0,1)|(-1,0)|(0,-1)
@   |( - )							| die			| kills thread
A   |( - )							| 				| 
B   |(a - )							| bounce if		| (dx, dy) = a>0 ? (-dx, -dy) : (dx, dy)
C   |( - )							| 				| 
D   |( - )							| 				| 
E   |( - )							| 				| 
F   |( - )							| 				| 
G   |( - )							| 				| 
H   |( - )							| up-down if	| (dx, dy) = a ? (1, 0) : (-1, 0)
I   |( - )							| right-left if	| (dx, dy) = a ? (0, 1) : (0, -1)
J   |( - )							| 				| 
K   |( - )							| 				| 
L   |( - )							| 				| 
M   |( - )							| 				| 
N   |( - )							| 				| 
O   |(a b - b a b)					| over			| same as forth OVER
P   |(n - push(stack[n])			| pick			| same as forth PICK
Q   |( - )							| 				| 
R   |( - )							| 				| 
S   |( - )							| 				| 
T   |( - )							| 				| 
U   |( - )							| 				| 
V   |( - )							| 				| 
W   |( - )							| ccw-cw if		| (dx, dy) = (a > b) ? (dx, dy) = (-dy, dx) : (a < b) ? (dx, dy) = (dy, -dx) : (dx, dy)
X   |( - )							| wall			| (dx, dy) = (-dx, -dy); blocks p, g, j, ", ;
Y   |( - )							| 				| 
Z   |( - )							| 				| 
[   |( - )							| 				| (dx, dy) = (-dy, dx)
\   |(a b - b a)					| swap			| same as forth SWAP
]   |( - )							| 				| (dx, dy) = (dy, -dx)
^   |( - )							| 				| (dx, dy) = (0, 1)
_   |(a - )							| left-right if	| (dx, dy) = a ? (-1, 0) : 1, 0)
`   |(a b - a>b)					| greater than	| 
a   |( - 10)						| push 10		| 
b   |( - 11)						| push 11		| 
c   |( - 12)						| push 12		| 
d   |( - 13)						| push 13		| 
e   |( - 14)						| push 14		| 
f   |( - 15)						| push 15		| 
g   |(y x - get(x,y)				| get			| 
h   |( - )							| 				| 
i   |( - )							| 				| 
j   |(a - )							| jump			| (x, y) += (dx, dy)*(a-1)
k   |(n - )							| iterate		| (dx, dy) = (0, 0) but gets put back after n cycles
l   |( - )							| 				| 
m   |( - )							| 				| 
n   |(whole stack - )				| clear stack	| 
o   |( - )							| 				| 
p   |(a y x - )						| put			| put(x,y,a)
q   |( - )							| die			| kills thread
r   |( - )							| reflect		| (dx, dy) = (-dx, -dy)
s   |(char - )						| store			| char = get(x+dx, y+dy); (x, y) += (dx, dy)
t   |( - )							| fork			| clone: (child.dx, child.dy) = (-parent.dx, -parent.dy), otherwise identical
u   |( - )							| 				| 
v   |( - )							| go down		| (dx, dy) = (0, -1)
w   |(a b - )						| cw-ccw if		| (dx, dy) = (a > b) ? (dx, dy) = (dy, -dx) : (a < b) ? (dx, dy) = (-dy, dx) : (dx, dy)
x   |(dy dx - )						| set velocity	| (dx, dy) = (tos-1, tos)
y   |( - )							| 				| 
z   |( - )							| nop			| 
{   |( - )							| 				| 
|   |(a - )							| down-up if	| (dx, dy) = a ? (0, -1) : (0, 1)
}   |( - )							| 				| 
~   |( - )							| 				| 
