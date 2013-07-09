package.cpath = "/home/microwish/lua-misc/lib/?.so;" .. package.cpath

local misc = require("misc")

local s = "ABCabcDEF"

local r, m = misc.rtrim(s, "A..Z")

print(r, m)
