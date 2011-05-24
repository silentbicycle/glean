include config.mk

PROGS= gln_tokens gln_index gln
GLN_TOKENS_O=	alloc.o tokenize.o hash.o whash.o array.o
GLN_INDEX_O=	alloc.o ignore.o hash.o whash.o db.o eta.o fhash.o \
		dumphex.o nextline.o stopword.o array.o
GLN_O=	alloc.o whash.o hash.o db.o dumphex.o nextline.o array.o
COPTS+= -Wall -pedantic

all: ${PROGS}

.c.o:
	${CC} -c $? ${COPTS}

clean:
	rm -f *.o gmon.out *.core ${PROGS} TAGS

TAGS:   *.c *.h
	etags *.[ch]

gln_tokens: gln_tokens.c ${GLN_TOKENS_O}
	${CC} -o $@ gln_tokens.c ${GLN_TOKENS_O} -lm ${COPTS}

gln_index: gln_index.c ${GLN_INDEX_O}
	${CC} -o $@ gln_index.c ${GLN_INDEX_O} ${COPTS} -lz -lm ${PROFILE}

gln: gln.c ${GLN_O}
	${CC} -o $@ gln.c ${GLN_O} ${COPTS} -lz

install:
	install ${PROGS} ${PREFIX}bin

uninstall:
	rm -f ${PREFIX}bin/gln_{index,tokens}
	rm -f ${PREFIX}bin/gln

lint:
	lint *.c

gln_tests: tests/tests.c
	${CC} -o $@ tests/tests.c -I${CHECK_INC_PATH} -L${CHECK_LIB_PATH} -lcheck


*.c: glean.h alloc.h Makefile

array.c: array.h
db.c: db.h gln_index.h whash.h
fhash.c: hash.h fhash.h 
gln.c:  hash.h whash.h gln.h
gln_index.c: gln_index.h
hash.c: hash.h
ignore.c: ignore.h
stopword.c: stopword.h hash.h whash.h gln_index.h
tokenize.c: tokenize.h whash.h
whash.c: tokenize.h whash.h hash.h
