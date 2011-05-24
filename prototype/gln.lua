#!/usr/bin/env lua

dofile("index")

function token_files(t)
   local fids, fs = words[t] or {}, {}
   for _,fid in ipairs(fids) do
      fs[#fs+1] = assert(files[fid])
   end
   return fs
end

function glean(t)
   local fs = token_files(t)
   print(table.concat(fs, " "))
end

glean(arg[1])
