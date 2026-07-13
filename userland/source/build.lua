local args = {...}

if #args < 1 then
    print("Usage: build.lua <source.c>")
    os.exit(1)
end

local source = args[1]
local output = source:gsub("%.c$", "")

local cmd = string.format(
    "/bin/tcc -static %s -L/usr/lib -o %s",
    source,
    output
)

print("Running: " .. cmd)

local ok = os.execute(cmd)

if ok then
    print("Built " .. output)
else
    print("Build failed")
    os.exit(1)
end
