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

all: libs
	${CMD} bin/utils.o bin/counter.o bin/board.o bruteforce_connect_four.c -o bfcf
	${CMD} -o bin/samples samples.c
	${CMD} bin/utils.o -o bin/merge merge.c

tidy:
	# removing trailing whitespace
	sed -i -e 's/[ \t]*$$//' *.h *.c

clean:
	rm -f bin/*

libs:
	${CMD} -c utils.c -o bin/utils.o
	${CMD} -c counter.c -o bin/counter.o
	${CMD} -c board.c -o bin/board.o
