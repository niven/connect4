ifeq (${MODE},)
else
	MODEFLAG=-D${MODE}
endif

CC=clang
DONTCARE_WARNINGS= -Wno-padded
CFLAGS=-std=c99 -g -Weverything -pedantic ${DONTCARE_WARNINGS} -O0 ${MODEFLAG}
CMD=${CC} ${CFLAGS}

all: clean libs db
	${CMD} bin/utils.o bin/bplustree.o bin/board63.o bin/board.o bpt_test.c -o bpt
	${CMD} bin/utils.o bin/counter.o bin/board63.o bin/board.o bruteforce_connect_four.c -o bfcf

db: clean libs
	${CMD} bin/utils.o bin/board.o bin/board63.o bin/bplustree.o db_utils.c -o db


clean:
	rm -rf *.c4_table *.c4_index bpt db bfcf *.gc *.o bin/*.o

libs: clean
	${CMD} -c bplustree.c -o bin/bplustree.o
	${CMD} -c utils.c -o bin/utils.o
	${CMD} -c counter.c -o bin/counter.o
	${CMD} -c board.c -o bin/board.o
	${CMD} -c board63.c -o bin/board63.o
