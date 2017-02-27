CFLAGS = -Wall
final: main.o
	gcc -o final main.o -lpthread

main.o: main.c
	gcc ${CFLAGS} -c main.c
	
clean:
	rm -rf *.o *.ko final
