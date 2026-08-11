-- Mesen2 test-runner script: run a number of frames, optionally hold input,
-- save a framebuffer screenshot, exit.
--
-- Env vars (read via os.getenv):
--   SHOT_FRAMES : frames to run before the screenshot (default 120)
--   SHOT_PATH   : output PNG path (default build/shot.png)
--   SHOT_INPUT  : comma-separated buttons to hold from frame 30 onward,
--                 e.g. "start" or "right,a" (default none)

local frames = tonumber(os.getenv("SHOT_FRAMES") or "120")
local path = os.getenv("SHOT_PATH") or "build/shot.png"
local inputSpec = os.getenv("SHOT_INPUT") or ""

local held = {}
for name in string.gmatch(inputSpec, "[^,]+") do
  held[name] = true
end

local count = 0

local function onFrame()
  count = count + 1
  if next(held) ~= nil and count >= 30 then
    emu.setInput({ a = held["a"] or false,
                   b = held["b"] or false,
                   select = held["select"] or false,
                   start = held["start"] or false,
                   up = held["up"] or false,
                   down = held["down"] or false,
                   left = held["left"] or false,
                   right = held["right"] or false }, 0)
  end
  if count >= frames then
    local png = emu.takeScreenshot()
    local f = io.open(path, "wb")
    f:write(png)
    f:close()
    emu.stop(0)
  end
end

emu.addEventCallback(onFrame, emu.eventType.startFrame)
