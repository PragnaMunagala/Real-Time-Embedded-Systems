CFLAGS = -Wall
out: main.o
	gcc -o out main.o -lm

main.o: main.c
	gcc ${CFLAGS} -c main.c
	
clean:
	rm -rf *.o *.ko out