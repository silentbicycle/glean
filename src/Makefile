########################################################################
# You may need to change some options in config.mk. 
include config.mk
########################################################################

PROGS= 		gln gln_filter gln_index gln_tokens test_gln

COMMON_O=	alloc.o array.o db.o dumphex.o nextline.o set.o word.o
GLN_O=		
GLN_FILTER_O=	
GLN_INDEX_O=	eta.o fname.o stopword.o
GLN_TOKENS_O=	tokenize.o

SUITES=		test_array.o test_eta.o test_set.o
TEST_O=		${COMMON_O} ${GLN_INDEX_O} ${GLN_FILTER_O} ${GLN_O} ${SUITES}


LIBS=		-lm -lz
LDFLAGS+=	${LIBS}

all: ${PROGS}

.c.o:
	${CC} -c $? ${COPTS}

clean:
	rm -f *.o gmon.out *.core ${PROGS} TAGS

TAGS:   *.c *.h
	etags *.[ch]

gln: gln.c ${COMMON_O} ${GLN_O}
	${CC} -o $@ gln.c ${COMMON_O} ${GLN_O} ${COPTS} ${LDFLAGS}

gln_filter: gln_filter.c ${COMMON_O} ${GLN_FILTER_O}
	${CC} -o $@ gln_filter.c ${COMMON_O} ${GLN_FILTER_O} \
	${COPTS} ${LDFLAGS} ${PROFILE}

gln_index: gln_index.c ${COMMON_O} ${GLN_INDEX_O}
	${CC} -o $@ gln_index.c ${COMMON_O} ${GLN_INDEX_O} ${PROFILE} \
	${COPTS} ${LDFLAGS}

gln_tokens: gln_tokens.c ${COMMON_O} ${GLN_TOKENS_O}
	${CC} -o $@ gln_tokens.c ${COMMON_O} ${GLN_TOKENS_O} \
	${COPTS} ${LDFLAGS}

test_gln: test.c ${TEST_O}
	${CC} -o $@ test.c ${TEST_O} ${COPTS} ${LDFLAGS}

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
db.c: db.h gln_index.h word.h
fname.c: set.h fname.h 
gln.c:  set.h word.h gln.h
gln_index.c: gln_index.h
gln_filter.c: alloc.h nextline.h array.h
set.c: set.h
stopword.c: stopword.h set.h word.h gln_index.h
tokenize.c: tokenize.h word.h
word.c: tokenize.h word.h set.h
