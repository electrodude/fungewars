CFLAGS=-std=c99 -O2 -g
CXXFLAGS=-std=c++11 -O2 -g -fpermissive
LDFLAGS=-L/usr/lib64/ -L/usr/lib64/mesa/ -lglut -lGL -lGLU -lm -lpthread -lpng
CC=gcc
CXX=g++
LD=g++

all:		fungewars

clean:		
		rm -f fungewars fungewars.o

fungewars:	fungewars.o
		${LD} ${LDFLAGS} -o $@ $^

%.o:		%.c
		${CC} ${CFLAGS} -c -o $@ $^

%.o:		%.cpp
		${CXX} ${CXXFLAGS} -c -o $@ $^