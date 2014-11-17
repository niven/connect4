CC=clang
DONTCARE_WARNINGS=-Wno-sign-compare -Wno-missing-prototypes -Wno-padded -Wno-sign-conversion
CFLAGS=-Weverything -pedantic ${DONTCARE_WARNINGS} -O3
CMD=${CC} ${CFLAGS}

all: clean libs
	${CMD} bin/bplustree.o bpt_test.c -o bpt
	${CMD} bin/utils.o bin/counter.o bin/board63.o bin/board.o bruteforce_connect_four.c -o bfcf

clean:
	rm -rf bfcf *.gc *.o bin/*.o

libs: clean
	${CMD} -c bplustree.c -o bin/bplustree.o
	${CMD} -c utils.c -o bin/utils.o
	${CMD} -c counter.c -o bin/counter.o
	${CMD} -c board.c -o bin/board.o
	${CMD} -c board63.c -o bin/board63.o
