-- Automated boot smoke test: run the ROM for a couple seconds, dump a
-- screenshot, then quit. Used to verify the build pipeline produces a ROM
-- that actually boots and renders, without needing a human at the emulator.
for i = 1, 120 do
    emu.frameadvance()
end

gui.savescreenshot("build/smoke_test.png")

emu.print("smoke test: screenshot captured")
os.exit()
