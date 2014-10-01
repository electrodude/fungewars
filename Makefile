CFLAGS=-std=c99 -O2 -g
#CXXFLAGS=-std=c++11 -O2 -g -fpermissive
LDFLAGS=-L/usr/lib/ -L/usr/lib/mesa/ -lglut -lGL -lGLU -lm -lpthread -lpng
CC=gcc
#CXX=g++
LD=ld

all:		fungewars

clean:		
		rm -f fungewars fungewars.o

fungewars:	fungewars.o
		${LD} -o $@ $^ ${LDFLAGS}

%.o:		%.c
		${CC} ${CFLAGS} -c -o $@ $^

#%.o:		%.cpp
#		${CXX} ${CXXFLAGS} -c -o $@ $^
