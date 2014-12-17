ifeq (${MODE},)
else
	MODEFLAG=-D${MODE}
endif

CC=clang
DONTCARE_WARNINGS= -Wno-padded -Wno-format-nonliteral
CFLAGS=-std=c99 -g -Weverything -pedantic ${DONTCARE_WARNINGS} -O3 ${MODEFLAG}
CMD=${CC} ${CFLAGS}

all: libs db
	${CMD} bin/utils.o bin/bplustree.o bin/board.o bpt_test.c -o bpt
	${CMD} bin/utils.o bin/counter.o bin/board.o bin/bplustree.o bruteforce_connect_four.c -o bfcf

db: libs
	${CMD} bin/utils.o bin/board.o bin/bplustree.o db_utils.c -o db


clean:
	rm -rf bpt db bfcf *.gc *.o bin/*.o

libs:
	${CMD} -c bplustree.c -o bin/bplustree.o
	${CMD} -c utils.c -o bin/utils.o
	${CMD} -c counter.c -o bin/counter.o
	${CMD} -c board.c -o bin/board.o
