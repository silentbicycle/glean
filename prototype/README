This code is not executed while running glean - it's just here for historical purposes.
You don't need Lua to run glean (though it's a fine language).

glean started out like this. I didn't keep the original, but rewrote it in 10 minutes or
so while discussing the design with Darius Bacon.

The single biggest difference is that glean uses mmap'd hash tables,
rather than reading in the whole data set, so that lookups are very fast.
