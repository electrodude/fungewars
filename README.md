Fungewars
=========

[Corewars](http://www.corewars.org/) in [Funge](http://quadium.net/funge/spec98.html)

# Documentation

TODO

# User Interface

TODO

# Instruction List

TODO

|char|stack                         |op
|----|------------------------------|----
|spc |( - )							| nop
|!   |(a - !a)						|
|"   |( - string)					| pushes cells until it hits another ", then goes back to normal mode
|#   |( - )							| (x, y) += (dx, dy)
|$   |(a - )						|
|%   |(a b - a%b)					|
|&   |( - )							|
|'   |( - get(x+dx, y+dy))			| (x, y) += (dx, dy)
|(   |( - )							|
|)   |( - )							|
|*   |(a b - a*b)					|
|+   |(a b - a+b)					|
|,   |( - )							|
|-   |(a b - a-b)					|
|.   |( - )							|
|/   |(a b - a/b)					|
|0   |( - 0)						|
|1   |( - 1)						|
|2   |( - 2)						|
|3   |( - 3)						|
|4   |( - 4)						|
|5   |( - 5)						|
|6   |( - 6)						|
|7   |( - 7)						|
|8   |( - 8)						|
|9   |( - 9)						|
|:   |(a - a a)						|
|;   |( - )							| jump: pretends everything is a nop until it hits another ;
|<   |( - )							| (dx, dy) = (-1, 0)
|=   |( - )							|
|>   |( - )							| (dx, dy) = ( 1, 0)
|?   |( - )							| (dx, dy) = (1,0)/(0,1)/(-1,0)/(0,-1)
|@   |( - )							| kills thread
|A   |( - )							|
|B   |(a - )						| (dx, dy) = a>0 ? (-dx, -dy) : (dx, dy)
|C   |( - )							|
|D   |( - )							|
|E   |( - )							|
|F   |( - )							|
|G   |( - )							|
|H   |( - )							| (dx, dy) = a ? (1, 0) : (-1, 0)
|I   |( - )							| (dx, dy) = a ? (0, 1) : (0, -1)
|J   |( - )							|
|K   |( - )							|
|L   |( - )							|
|M   |( - )							|
|N   |( - )							|
|O   |(a b - b a b)					| forth OVER
|P   |(n - push(stack[n])			| forth PICK
|Q   |( - )							|
|R   |( - )							|
|S   |( - )							|
|T   |( - )							|
|U   |( - )							|
|V   |( - )							|
|W   |( - )							| (dx, dy) = (a > b) ? (dx, dy) = (-dy, dx) : (a < b) ? (dx, dy) = (dy, -dx) : (dx, dy)
|X   |( - )							| (dx, dy) = (-dx, -dy); blocks p, g, j, ", ;
|Y   |( - )							|
|Z   |( - )							|
|[   |( - )							| (dx, dy) = (-dy, dx)
|\   |(a b - b a)					|
|]   |( - )							| (dx, dy) = (dy, -dx)
|^   |( - )							| (dx, dy) = (0, 1)
|_   |(a - )						| (dx, dy) = a ? (-1, 0) : 1, 0)
|`   |(a b - a>b)					|
|a   |( - 10)						|
|b   |( - 11)						|
|c   |( - 12)						|
|d   |( - 13)						|
|e   |( - 14)						|
|f   |( - 15)						|
|g   |(y x - get(x,y)				|
|h   |( - )							|
|i   |( - )							|
|j   |(a - )						| (x, y) += (dx, dy)*(a-1)
|k   |(n - )						| iterate: (dx, dy) = (0, 0) but gets put back after n cycles
|l   |( - )							|
|m   |( - )							|
|n   |(whole stack - )				|
|o   |( - )							|
|p   |(a y x - )					| put(x,y,a)
|q   |( - )							| kills thread
|r   |( - )							| (dx, dy) = (-dx, -dy)
|s   |(char - )						| char = get(x+dx, y+dy); (x, y) += (dx, dy)
|t   |( - )							| clone: (child.dx, child.dy) = (-parent.dx, -parent.dy), otherwise identical
|u   |( - )							|
|v   |( - )							| (dx, dy) = (0, -1)
|w   |(a b - )						| (dx, dy) = (a > b) ? (dx, dy) = (dy, -dx) : (a < b) ? (dx, dy) = (-dy, dx) : (dx, dy)
|x   |(dy dx - )					| (dx, dy) = (tos-1, tos)
|y   |( - )							|
|z   |( - )							| nop
|{   |( - )							|
||   |(a - )						| (dx, dy) = a ? (0, -1) : (0, 1)
|}   |( - )							|
|~   |( - )							|
