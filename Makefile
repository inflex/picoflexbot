CFLAGS=-Wall -g -I. -O2 

OBJ=picoflexbot
default: picoflexbot

.c.o:
	${CC} ${CFLAGS} $(COMPONENTS) -c $*.c

all: ${OBJ} 

picoflexbot: picoflexbot.c 
	${CC} ${CFLAGS} picoflexbot.c -o picoflexbot 

clean:
	rm -f *.o *core 
