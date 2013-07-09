--package.cpath = package.cpath .. ";/home/microwish/lua-mcpack/lib/?.so"
package.cpath = "/home/microwish/lua-misc/lib/?.so;" .. package.cpath

local misc = require("misc")

--local arr = { 1 }

--local packed = misc.pack('i*', arr)
--local packed = misc.pack('nvc*', 0x1234, 0x5678, 65, 66)

--print(string.byte(packed, 1, #packed))

local packed = "\\x04\\x00\\xa0\\x00"
--local packed = "\x04\x00\xa0\x00"
--local packed = "\\4\\0\\16\0";
--local packed = "40160";
local fmt = "c2chars/nint"
local arr = misc.unpack(fmt, packed)
for k, v in pairs(arr) do
  print(k, v)
end

--print(tonumber("\\x04\\x00\\xa0\\x00", 16))
