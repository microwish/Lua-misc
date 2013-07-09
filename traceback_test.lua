package.cpath = "/home/microwish/lua-misc/lib/?.so;" .. package.cpath

local misc = require("misc")

function p_dbg()
  local r = misc.traceback_retarr()
	print(r, #r)

	for k, v in pairs(r) do
		for k1, v1 in pairs(v) do print(k1, v1) end
	end
end

p_dbg()
