.SUFFIXES: .1 .html

all: glean.html gln.html gln_filter.html gln_index.html

.1.html:
	mandoc -Thtml $< > $@

glean.html: glean.1
gln.html: gln.1
gln_filter.html: gln_filter.1
gln_index.html: gln_index.1

clean:
	rm -f *.html
