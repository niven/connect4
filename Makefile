ifeq (${MODE},)
else
	MODEFLAG=-D${MODE}
endif

ifeq (${ARCH},32)
	ARCHFLAGS=-m32 -DBUILD_32_BITS
else
endif


CC=clang
DONTCARE_WARNINGS= -Wno-padded -Wno-format-nonliteral
CFLAGS=-std=c99 -g -Weverything -pedantic -ferror-limit=3 ${DONTCARE_WARNINGS} -O3 ${MODEFLAG} ${ARCHFLAGS}
CMD=${CC} ${CFLAGS}

all: libs db
	${CMD} bin/utils.o bin/bplustree.o bin/board.o bpt_test.c -o bpt
	${CMD} bin/utils.o bin/counter.o bin/board.o bin/bplustree.o store_boards.c -o store_boards
	${CMD} bin/utils.o bin/counter.o bin/board.o bin/bplustree.o bruteforce_connect_four.c -o bfcf

tools:
	${CMD} -o tools/samples samples.c
	${CMD} -o tools/merge merge.c

db: libs
	${CMD} bin/utils.o bin/board.o bin/bplustree.o db_utils.c -o db

tidy:
	# removing trailing whitespace
	sed -i -e 's/[ \t]*$$//' *.h *.c

clean:
	rm -rf bpt db bfcf *.gc *.o bin/*.o tools/*

libs:
	${CMD} -c bplustree.c -o bin/bplustree.o
	${CMD} -c utils.c -o bin/utils.o
	${CMD} -c counter.c -o bin/counter.o
	${CMD} -c board.c -o bin/board.o
