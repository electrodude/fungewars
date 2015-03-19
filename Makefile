CFLAGS=-std=c99 -O2 -g
#CXXFLAGS=-std=c++11 -O2 -g -fpermissive
LDFLAGS=-L/usr/lib64/ -L/usr64/lib/mesa/ -lglut -lGL -lGLU -lm -lpthread -lpng
CC=gcc
#CXX=g++

# should use ld but it won't work, have to figure this out
LD=g++

all:		fungewars

clean:		
		rm -f fungewars fungewars.o

fungewars:	fungewars.o interpreter.o graphics.o
		${LD} -o $@ $^ ${LDFLAGS}

fungewars.o:	interpreter.h graphics.h

graphics.o:	fungewars.h

interpreter.o:	graphics.h

%.o:		%.c %.h
		${CC} ${CFLAGS} -c -o $@ $<

#%.o:		%.cpp %.hpp
#		${CXX} ${CXXFLAGS} -c -o $@ $<
