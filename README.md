Glean is a lightweight search engine for local text docs.

Run `gln_index` to build an index, then use `gln` to search.

To install:

Look at `src/config.mk`. (You probably won't need to edit it.)
    
    cd src
    make
    make install

Usage:

    gln_index -p     # index all text-ish files in ~, store index in ~./gln/; -p = show-progress

    gln foo bar OR baz          # show all lines containing foo AND (bar OR baz)

For more info, look at the man pages or HTML docs in doc/.

It's like `grep -r ~ foo`, but much faster.

With many search indexing systems, the index can be 30-50% the size of
indexed content. Glean's index is a svelte 2-5%, and builds rapidly.

Glean falls back on grep at the file level, and is much more efficient for
searching lots of small-to-medium files, rather than a few larger (25+ MB) ones.

The prototype/ directory contains the original prototype (in Lua). Glean
follows this essential design, the rest is implementation details.
