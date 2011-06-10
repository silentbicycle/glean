include config.mk

PROGS= gln_tokens gln_index gln_filter gln
GLN_TOKENS_O=	alloc.o tokenize.o hash.o whash.o array.o
GLN_INDEX_O=	alloc.o hash.o whash.o db.o eta.o fhash.o \
		dumphex.o nextline.o stopword.o array.o
GLN_O=	alloc.o whash.o hash.o db.o dumphex.o nextline.o array.o
GLN_FILTER_O=	alloc.o array.o nextline.o
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

gln_filter: gln_filter.c ${GLN_FILTER_O}
	${CC} -o $@ gln_filter.c ${GLN_FILTER_O} ${COPTS} ${PROFILE}

gln: gln.c ${GLN_O}
	${CC} -o $@ gln.c ${GLN_O} ${COPTS} -lz

#TODO: actually install man pages.
install:
	install ${PROGS} ${PREFIX}bin
	install ../doc/*.1 ${MANPATH1}

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
gln_filter.c: alloc.h nextline.h array.h
hash.c: hash.h
stopword.c: stopword.h hash.h whash.h gln_index.h
tokenize.c: tokenize.h whash.h
whash.c: tokenize.h whash.h hash.h
