-- MAME Lua trace script for RC702 BIOS loader verification
-- Uses CPU debug breakpoints (requires MAME debug build or -debug flag)

local phase = 0

-- We can't set breakpoints from periodic callbacks easily.
-- Instead, poll at frame rate and check key milestones by reading memory.

local last_phase_time = 0

emu.register_periodic(function()
    local cpu = manager.machine.devices[":maincpu"]
    if not cpu then return end
    local space = cpu.spaces["program"]
    local pc = cpu.state["PC"].value
    local sp = cpu.state["SP"].value
    local t = manager.machine.time.seconds

    -- Phase 0: Wait for loader to be loaded (0x0280 has 0xF3 = DI)
    if phase == 0 then
        local b = space:read_u8(0x0280)
        if b == 0xF3 and t > 0.5 then
            phase = 1
            print(string.format("\n=== PHASE 1: Loader loaded at t=%.2f ===", t))
            print(string.format("  0x0280 = 0x%02X (DI) — correct", b))
            -- Verify LDIR parameters
            local src = space:read_u8(0x0282) + space:read_u8(0x0283) * 256
            local dst = space:read_u8(0x0285) + space:read_u8(0x0286) * 256
            local cnt = space:read_u8(0x0288) + space:read_u8(0x0289) * 256
            print(string.format("  LDIR: src=0x%04X dst=0x%04X count=%d", src, dst, cnt))
            -- Verify LD SP
            local sp_target = space:read_u8(0x02FF) + space:read_u8(0x0300) * 256
            print(string.format("  LD SP target: 0x%04X", sp_target))
            -- Verify JP target
            local jp_target = space:read_u8(0x0302) + space:read_u8(0x0303) * 256
            print(string.format("  JP target: 0x%04X", jp_target))
        end
        return
    end

    -- Phase 1: Wait for BIOS to appear at 0xC000
    if phase == 1 then
        local b = space:read_u8(0xC000)
        if b == 0xC3 then
            phase = 2
            print(string.format("\n=== PHASE 2: BIOS at 0xC000 (head 0 copy done) at t=%.2f ===", t))
            -- Verify all 17 JP entries
            local ok = true
            for i = 0, 16 do
                local a = 0xC000 + i * 3
                local op = space:read_u8(a)
                local tgt = space:read_u8(a+1) + space:read_u8(a+2) * 256
                if op ~= 0xC3 then
                    print(string.format("  ERROR: entry %d at 0x%04X = 0x%02X, not JP", i, a, op))
                    ok = false
                end
            end
            if ok then print("  All 17 JP entries verified") end

            -- Show first entry target (bios_boot)
            local boot_target = space:read_u8(0xC001) + space:read_u8(0xC002) * 256
            print(string.format("  BOOT target: JP 0x%04X", boot_target))
        end
        return
    end

    -- Phase 2: Wait for PC to be at/near 0xC000 (BIOS executing) or SP=0xF500
    if phase == 2 then
        if sp == 0xF500 or (pc >= 0xC000 and pc < 0xF000) then
            phase = 3
            print(string.format("\n=== PHASE 3: BIOS executing at t=%.2f ===", t))
            print(string.format("  PC=0x%04X SP=0x%04X I=0x%02X", pc, sp, cpu.state["I"].value))

            -- Full verification
            print("\n--- Full Memory Verification ---")

            -- BIOS jump table
            print("BIOS JP table:")
            local names = {"BOOT","WBOOT","CONST","CONIN","CONOUT","LIST","PUNCH",
                          "READER","HOME","SELDSK","SETTRK","SETSEC","SETDMA",
                          "READ","WRITE","LISTST","SECTRAN"}
            for i = 0, 16 do
                local a = 0xC000 + i * 3
                local tgt = space:read_u8(a+1) + space:read_u8(a+2) * 256
                print(string.format("  +%02X %-8s JP 0x%04X", i*3, names[i+1], tgt))
            end

            -- JTVARS
            print("\nJTVARS at 0xC033:")
            local jt = ""
            for i = 0, 21 do jt = jt .. string.format("%02X ", space:read_u8(0xC033 + i)) end
            print("  " .. jt)

            -- Stack area
            print(string.format("\nStack at SP=0x%04X:", sp))
            for i = 0, 3 do
                local a = sp + i*2
                print(string.format("  0x%04X: 0x%04X",
                    a, space:read_u8(a) + space:read_u8(a+1)*256))
            end

            -- Display buffer (should be spaces=0x20 after console_init)
            local disp = ""
            local disp_hex = ""
            for i = 0, 39 do
                local b = space:read_u8(0xF800 + i)
                disp_hex = disp_hex .. string.format("%02X ", b)
                if b >= 32 and b < 127 then disp = disp .. string.char(b)
                else disp = disp .. "." end
            end
            print("\nDisplay 0xF800 (first 40 bytes):")
            print("  hex: " .. disp_hex)
            print("  asc: " .. disp)

            -- Vector table
            print("\nVector table at 0xF600:")
            local vt = ""
            for i = 0, 35 do vt = vt .. string.format("%02X ", space:read_u8(0xF600 + i)) end
            print("  " .. vt)

            print(string.format("\nI register: 0x%02X", cpu.state["I"].value))
            print("\n=== VERIFICATION COMPLETE ===")
        end
        return
    end
end, "trace")
