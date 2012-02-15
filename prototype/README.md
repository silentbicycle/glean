This code is not executed while running glean - it's just here for historical purposes.
You don't need Lua to run glean (though it's a fine language).

glean started out like this. I didn't keep the original, but rewrote it in 10 minutes or
so while discussing the design with Darius Bacon.

The prototype re-reads the whole data set on every use. glean uses
memory-mapped, compressed hash tables to keep the index small and
make lookups significantly faster.
