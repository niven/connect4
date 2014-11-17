CC=clang
CFLAGS=-Weverything -pedantic -O3
CMD=${CC} ${CFLAGS}

all: clean libs
	${CMD} bin/bplustree.o bpt_test.c -o bpt
	${CMD} bin/c4_index.o c4_index_test.c -o c4it
	${CMD} bin/utils.o bin/counter.o bin/board63.o bin/board.o bruteforce_connect_four.c -o bfcf

clean:
	rm -rf bfcf *.gc *.o bin/*.o

libs:
	${CMD} -c bplustree.c -o bin/bplustree.o
	${CMD} -c c4_index.c -o bin/c4_index.o
	${CMD} -c utils.c -o bin/utils.o
	${CMD} -c counter.c -o bin/counter.o
	${CMD} -c board.c -o bin/board.o
	${CMD} -c board63.c -o bin/board63.o
