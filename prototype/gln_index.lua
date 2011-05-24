#!/usr/bin/env lua

function uniq_words(fn)
   local f = assert(io.open(fn))
   local dat, ws = f:read("*a"), {}
   for w in dat:gmatch("(%w+)[^%w]*") do
      ws[w] = true
   end
   return ws
end

local files, words = {}, {}

while true do
   local fn = io.stdin:read()
   if fn then
      local ws = uniq_words(fn)
      local fid = #files+1
      files[fid] = fn
      for w in pairs(ws) do
         local wl = words[w] or {}
         wl[#wl+1] = fid
         words[w] = wl
      end
   else
      break
   end
end

local index_file = assert(io.open("index", 'w'))

function w(...) index_file:write(string.format(...)) end
w"files = {\n"
for i,fn in ipairs(files) do
   w("    %q,\n", fn)
end
w"}\n\n"

w"words = {\n"
for word,wl in pairs(words) do
   w("    [%q]={", word)
   for _,fid in ipairs(wl) do w("%d,", fid) end
   w("},\n")
end
w"}\n"

os.execute("touch timestamp")
