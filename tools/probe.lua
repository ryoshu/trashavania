-- probe.lua -- headless fceux test driver for Trashavania.
--
-- Reads a scripted input/check sequence from the PROBE_SCRIPT env var,
-- runs it via a per-frame callback, prints PASS/FAIL data to stdout.
--
-- PROBE_SCRIPT commands (semicolon separated):
--   wait:N            advance N frames
--   hold:btns         hold buttons (comma list: a,b,start,select,up,down,left,right)
--   release           release all buttons
--   tap:btns          press for 2 frames then release
--   shot:path.ppm     dump screen to PPM (see tools/screen2png.py)
--   read:name:addr    print "VAR name = value" (addr hex)
--   quit              exit emulator
--
-- Run: fceux --sound 0 --loadlua tools/probe.lua build/trashavania.nes

local script = os.getenv("PROBE_SCRIPT") or "wait:60;quit"
local cmds = {}
for c in string.gmatch(script, "[^;]+") do cmds[#cmds + 1] = c end

local held = {}

local function parse_buttons(s)
  local t = {}
  for b in string.gmatch(s, "[^,]+") do t[b] = true end
  return t
end

local function dump_screen(path)
  local f, err = io.open(path, "wb")
  if not f then
    io.write("SHOT_FAIL " .. path .. " " .. tostring(err) .. "\n")
    return
  end
  f:write("P6\n256 240\n255\n")
  for y = 0, 239 do
    local row = {}
    for x = 0, 255 do
      local r, g, b = emu.getscreenpixel(x, y, true)
      row[#row + 1] = string.char(r, g, b)
    end
    f:write(table.concat(row))
  end
  f:close()
  io.write("SHOT " .. path .. "\n")
end

local ci = 1
local waitleft = 0
local tapleft = 0

local function step()
  while true do
    if waitleft > 0 then
      waitleft = waitleft - 1
      return
    end
    if tapleft > 0 then
      tapleft = tapleft - 1
      if tapleft == 0 then held = {} end
      return
    end
    local cmd = cmds[ci]
    if cmd == nil then return end
    ci = ci + 1
    local op, arg = string.match(cmd, "^(%w+):?(.*)$")
    if op == "wait" then
      waitleft = tonumber(arg)
      return
    elseif op == "hold" then
      held = parse_buttons(arg)
    elseif op == "release" then
      held = {}
    elseif op == "tap" then
      held = parse_buttons(arg)
      tapleft = 2
      return
    elseif op == "shot" then
      dump_screen(arg)
    elseif op == "snd" then
      local s = sound.get()
      io.write(string.format(
        "SND p1v=%.2f p1f=%.0f p2v=%.2f triv=%.2f trif=%.0f noiv=%.2f\n",
        s.rp2a03.square1.volume, s.rp2a03.square1.frequency,
        s.rp2a03.square2.volume, s.rp2a03.triangle.volume,
        s.rp2a03.triangle.frequency, s.rp2a03.noise.volume))
    elseif op == "dump" then
      local a, n = string.match(arg, "^(%x+):(%d+)$")
      a = tonumber(a, 16)
      local out = {}
      for k = 0, tonumber(n) - 1 do
        out[#out + 1] = string.format("%02X", memory.readbyte(a + k))
      end
      io.write("DUMP " .. string.format("%04X", a) .. " " ..
               table.concat(out, " ") .. "\n")
    elseif op == "read" then
      local name, addr = string.match(arg, "^([%w_]+):(%x+)$")
      io.write(string.format("VAR %s = %d\n", name,
               memory.readbyte(tonumber(addr, 16))))
    elseif op == "quit" then
      io.write("DONE\n")
      os.exit(0)
    end
  end
end

emu.speedmode("maximum")

local function onframe()
  step()
  joypad.set(1, {
    A = held["a"], B = held["b"], select = held["select"],
    start = held["start"], up = held["up"], down = held["down"],
    left = held["left"], right = held["right"],
  })
end

emu.registerafter(onframe)
