Answers for the clean room FDC reimplementation

1. MSR polling around RECALIBRATE and SEEK

Both the BIOS and PROM use the same MSR polling pattern for every command/parameter byte:

- Before writing a command or parameter byte: poll MSR until bits 7:6 == 10 (RQM=1, DIO=0 — "FDC ready to receive from CPU"). The mask is 0xC0, expected
  value 0x80.
- Before reading a result byte: poll MSR until bits 7:6 == 11 (RQM=1, DIO=1 — "FDC has data for CPU"). Mask 0xC0, expected 0xC0.

After RECALIBRATE and SEEK, the BIOS does not poll MSR for D0B (drive 0 busy) clearing. Instead it waits for the CTC Ch3 interrupt (the FDC's INT line  
is routed through a Z80-CTC channel). The ISR sets a flag (fl_flg in the BIOS, floppy_operation_completed_flag in the PROM), and the mainline busy-waits
on that flag.

Yes, both use SENSE INTERRUPT STATUS after recalibrate/seek. But here's the critical detail: the ISR itself decides what to do. When the interrupt      
fires:
1. ISR checks MSR bit 4 (CB — Controller Busy).
2. If CB=1 → calls the full 7-byte result read routine (for READ DATA etc.).
3. If CB=0 → calls SENSE INTERRUPT STATUS (for SEEK/RECALIBRATE completion).

So SENSE INTERRUPT is issued inside the ISR, not in mainline code after the interrupt. The mainline just waits for the flag, and by the time it checks,
ST0 and PCN are already populated.

Exception: in the BIOS's chktrk() retry path (recalibrate after failed seek), mainline code explicitly calls fdc_sense_int() after wfitr() returns —    
that's a second SENSE INTERRUPT in mainline, after the ISR already did one.

2. READ DATA handling

After sending the 9 command bytes (command + 8 parameters), the actual data transfer is handled entirely by DMA (Am9517A channel 1), not by CPU polling.
The sequence is:

1. DMA is set up before the command is sent: address register, word count, mode (IO→mem for reads), then unmask.
2. 9 command bytes sent to FDC via the write-when-ready polling (RQM=1, DIO=0 each byte).
3. DMA runs autonomously — the FDC's DRQ line drives the DMA controller. CPU does nothing during the data phase.
4. FDC fires INT when execution phase completes → CTC Ch3 ISR fires.
5. ISR checks CB (MSR bit 4). CB=1 means result phase → ISR reads all 7 result bytes using the same RQM+DIO=11 polling per byte, with CB (bit 4) checked
   after each byte to detect early termination.
6. ISR also reads DMA status register after the FDC result bytes.
7. ISR sets the completion flag; mainline wakes and checks st0/st1/st2.

So: no MSR polling between command and result. DMA handles the data phase. The interrupt signals transition to result phase. The ISR reads the result   
bytes.

3. Draining pending interrupts at boot

No. There is no explicit SENSE INTERRUPT drain loop at boot. The boot sequence is:

1. init_fdc(): delay 391ms, then poll MSR bits 4:0 (the D0B-D3B + CB flags) until all zero — while (fdc_status() & 0x1F) — then send SPECIFY command    
   (0x03, 0x4F, 0x20).
2. get_floppy_ready(): EI, motor on, then proceed to boot_from_floppy_or_jump_prom1().
3. That function does: 391ms motor spin-up delay, SENSE DRIVE STATUS, then RECALIBRATE + wait-for-interrupt + SENSE INTERRUPT (via the ISR).

The only "drain" is that init_fdc() waits for all the busy bits (D0B-D3B, CB) to clear before issuing SPECIFY. If the FDC had pending seek completions  
from the PROM's track 0 read, those bits would already be clear by the time the BIOS gets control (there's a substantial delay from BIOS loading). The  
CTC is freshly initialized, so no stale interrupts are pending in the CTC.

4. What does hal_in look like?

There is no function called hal_in. Port I/O uses the DEFPORT macro system which generates IN A,(n) — the fixed-port form (Z80 opcode DB n), not IN     
A,(C).

For SDCC: __sfr __at(addr) declarations make port reads/writes look like variable access. The compiler emits IN A,(n) and OUT (n),A directly.

For clang: address_space(2) pointer dereferences compile to the same IN A,(n) / OUT (n),A instructions.

The port addresses are compile-time constants: FDC status = 0x04, FDC data = 0x05. There is no IN A,(C) form used, and B is not involved. The only      
exception is port_in_rt(p) / port_out_rt(p, v) (clang-only, runtime port number), but the FDC code doesn't use that — all FDC port accesses are the   
constant-address form.                                                
-----

  ---                                                                                                                                                     
Result byte reading: reads until CB clears, NOT exactly 7

Both the BIOS and PROM use the same pattern: read a result byte, then check CB (MSR bit 4) to decide whether to continue. They do not blindly read 7    
bytes.

BIOS (bios.c:323-335): do-while loop up to 7 iterations. Each iteration:
1. fdc_read() — polls MSR for RQM=1+DIO=1, then reads data port
2. Short delay (4-iteration empty loop)
3. Check port_in(fdc_status) & 0b00010000 (CB bit). If CB=0, return immediately — don't read more bytes.
4. Decrement counter, loop if < 7.

PROM (rom.c:357-372): for loop up to 7 iterations. Each iteration:
1. fdc_read_when_ready() — polls MSR for RQM=1+DIO=1, reads data port
2. Check fdc_status() & 0b00010000 (CB bit). If CB=0, store DMA status in the next byte slot and return.
3. If all 7 bytes read and CB is still set → error halt (0xFE).

So the answer is: it reads until CB clears, with a maximum of 7. For a normal READ DATA result, the FDC produces exactly 7 result bytes (ST0, ST1, ST2,
C, H, R, N), so CB will clear after the 7th byte is read. For SENSE INTERRUPT STATUS (2 bytes) or SENSE DRIVE STATUS (1 byte), CB clears sooner and the
loop exits early.

The short delay in the BIOS between reading and checking CB is significant — it gives the FDC a few microseconds to update MSR after the data port read.
Without it, you might read CB before the FDC has deasserted it, and try to read a nonexistent 8th byte.

After the last result byte is read and CB clears, MSR shows RQM=1, DIO=0 (idle, ready for next command). The transition is immediate from the CPU's     
perspective once CB drops.

DMA port I/O: all OUT (n),A — never OUT (C),A

All DMA port access goes through the DEFPORT / port_out macro system. Both compilers produce OUT (n),A (opcode D3 nn):

- SDCC: __sfr __at(addr) declarations. Assigning to an __sfr variable emits OUT (n),A. The port address is an 8-bit immediate in the instruction. B is  
  irrelevant.
- Clang: address_space(2) pointer dereference with a (uint8_t) cast on the address. The compiler lowers this to OUT (n),A.

There is no OUT (C),A anywhere in the FDC or DMA code. The port_in_rt / port_out_rt forms (which could theoretically use IN A,(C) / OUT (C),A for       
runtime port selection) are clang-only and are not used by any FDC or DMA code.

So the 0xF0-0xFF DMA ports are fine — OUT (n),A puts the 8-bit port on A0-A7 and the accumulator contents on A8-A15 (as per Z80 behavior for the OUT    
(n),A instruction). The upper address bus byte is whatever was in A before the instruction, which doesn't matter because the RC702 hardware only decodes
the low 8 address bits for I/O. There's no risk of the (B<<8|port) aliasing issue that OUT (C),A would have.          

---
---
Does the working BIOS use explicit SEEK (0x0F)?

Yes, absolutely. Both the BIOS and PROM use explicit SEEK commands. There is no reliance on implied seek during READ DATA.

BIOS seek path

The BIOS's chktrk() (bios.c:398-443) is called before every sector read/write. It:

1. Checks if a seek is even needed: if (hstdsk == lstdsk && hsttrk == lsttrk) return; — skip seek if already on the right track.
2. If seek is needed:                                                                                                                                 
   - clfit() — clear interrupt flag (fl_flg = 0) with DI/EI                                                                                              
   - fdc_seek() — sends 3 bytes: 0x0F, drive, cylinder                                                                                                   
   - wfitr() — busy-waits on fl_flg until the ISR sets it, then clears it
3. ISR fires (CTC Ch3), ISR checks CB=0 → calls fdc_sense_int() inside the ISR → ST0 and PCN now in rstab.
4. Back in mainline, checks rstab.st0 == (drive | 0x20) (Seek End + correct drive).
5. On failure: recalibrate + wait-for-interrupt + mainline sense_int + seek again + wait-for-interrupt + mainline sense_int.

Note the subtle difference: the normal path gets SENSE INTERRUPT done inside the ISR (the ISR sees CB=0 and calls fdc_sense_int()). The retry path      
issues an additional mainline fdc_sense_int() after wfitr() returns — that's a second SENSE INTERRUPT after the ISR already did one. This is probably   
belt-and-suspenders, or possibly because the ISR's sense_int result might have been from the recalibrate, and mainline needs a fresh one.

PROM seek path

The PROM's fdc_select_drive_cylinder_head() (rom.c:399-401):
1. fdc_seek(head_and_drive, cylinder) — sends 0x0F + HD/US + NCN
2. verify_seek_result(expected_pcn) — calls wait_fdc_ready(0xFF) which polls floppy_operation_completed_flag in a delay loop (3ms per poll, 255         
   iterations max = ~765ms timeout)
3. ISR fires, reads SENSE INTERRUPT (because CB=0), sets flag
4. Mainline wakes, checks ST0 == (drive | 0x20) and PCN == expected cylinder

The READ DATA command does NOT have an implied seek

After chktrk() does the explicit seek, sec_rw() sends the READ DATA command with the cylinder parameter matching where the head already is. The µPD765  
READ DATA command does have a cylinder field, but it's for ID matching, not for seeking — if the head isn't on the right track, you get ST1.ND (No Data)
errors.

Does the CTC Ch3 interrupt actually fire in MAME?

Yes. The hardware wiring in the MAME RC702 driver is:

▎ UPD765A FDC @ ports 0x04-0x05, DRQ->DMA ch1, INTRQ->CTC1 TRG3

The FDC's INTRQ output is connected to CTC channel 3's trigger input (TRG3). CTC Ch3 is configured as a counter with time constant 1 and interrupts     
enabled (mode byte 0xD7, count 0x01). This means: one pulse on INTRQ → CTC counts down from 1 → hits zero → fires Z80 interrupt with vector offset 6  
(channel 3 × 2) from the CTC's vector base.

The working BIOS boots successfully in MAME — it reads Track 0, loads CP/M, does directory listings, runs programs. Every one of those operations goes  
through chktrk() → fdc_seek() → wfitr() (wait for interrupt) → sec_rw() → watir() (wait for interrupt). If the CTC Ch3 interrupt didn't fire, the BIOS
would hang at watir() on the very first disk access. The fact that CP/M boots and runs proves the interrupt chain works end-to-end in MAME.

There's even a GDB tracing script (rcbios-in-c/gdb_trace.py) that was written specifically to "monitor the floppy ISR to verify CTC ch3 interrupt       
delivery."
                 

---
---                                                                                                                                                     
1. CTC vector base: written once, to Ch0 port only

The Z80-CTC has a single shared interrupt vector that is written only to channel 0. This is a hardware property of the CTC — it's not per-channel. The  
vector word is written once during init, and the CTC internally adds the channel offset (0, 2, 4, 6) to generate the actual vector for each channel.

BIOS (bios_hw_init.c:153):                                                                                                                              
port_out(ctc0, 0x00);    ← vector base, written to port 0x0C (Ch0)

PROM (rom.c:140):                                                                                                                                       
ctc0_write(0x08);        ← vector base, written to port 0x0C (Ch0)

The vector base byte has bit 0 = 0, which is how the CTC recognizes it as a vector word (vs. a control word where bit 0 = 1). The vector is written to  
Ch0's port but applies to all four channels.

Your code writes 0x08 to port 0x0C — that's correct and matches the PROM. But double-check: are you writing it to port 0x0C (Ch0), or to port 0x0F      
(Ch3)? The vector must go to Ch0's port address. Writing a vector byte to Ch3's port would be ignored or misinterpreted.

2. Mode byte: yes, 0xD7 is correct

Both BIOS and PROM use 0xD7 for Ch3. Breaking it down:

┌─────┬───────┬────────────────────────────────┐                                                                                                        
│ Bit │ Value │            Meaning             │                                                                                                      
├─────┼───────┼────────────────────────────────┤                                                                                                        
│ 7   │ 1     │ Interrupt enabled              │
├─────┼───────┼────────────────────────────────┤                                                                                                        
│ 6   │ 0     │ Counter mode (not timer)       │                                                                                                      
├─────┼───────┼────────────────────────────────┤                                                                                                        
│ 5   │ 1     │ Rising edge                    │
├─────┼───────┼────────────────────────────────┤                                                                                                        
│ 4   │ 1     │ Time constant follows          │                                                                                                      
├─────┼───────┼────────────────────────────────┤                                                                                                        
│ 3   │ 0     │ (unused in counter mode)       │
├─────┼───────┼────────────────────────────────┤                                                                                                        
│ 2   │ 1     │ Software reset of this channel │                                                                                                      
├─────┼───────┼────────────────────────────────┤                                                                                                        
│ 1   │ 1     │ (control word)                 │
├─────┼───────┼────────────────────────────────┤                                                                                                        
│ 0   │ 1     │ (control word)                 │                                                                                                      
└─────┴───────┴────────────────────────────────┘

Time constant = 0x01 (interrupt on every pulse from µPD765 INTRQ).

This matches exactly in both the PROM (rom.c:147-148) and BIOS CONFI block (boot_confi.c:23).

3. The critical initialization sequence you may be missing

Here is the exact order the working BIOS uses (bios_hw_init.c:134-161):

1. setup_ivt() — copy IVT to page-aligned RAM, set I register, enable IM2. This happens BEFORE any device init.
2. PIO init (vectors + modes)
3. CTC init — and here's the critical sequence:                                                                                                         
   - port_out(ctc0, 0x00) — vector base to Ch0 first                                                                                                     
   - port_out(ctc0, mode0) then port_out(ctc0, count0) — Ch0                                                                                             
   - port_out(ctc1, mode1) then port_out(ctc1, count1) — Ch1                                                                                             
   - port_out(ctc2, mode2) then port_out(ctc2, count2) — Ch2                                                                                             
   - port_out(ctc3, mode3) then port_out(ctc3, count3) — Ch3
4. SIO, DMA, CRT, FDC init follow.

The PROM does the same thing (rom.c:138-149):
- ctc0_write(0x08) — vector base to Ch0
- Ch0 mode+count, Ch1 mode+count, Ch2 mode+count, Ch3 mode+count

What you might be missing:

- The vector base must be written to Ch0 before any channel mode words. If you write Ch3's mode before the vector base, the CTC may not know what vector
  to generate.
- IM2 + I register must be set before enabling interrupts on any CTC channel. The BIOS does setup_ivt() (which sets I register and IM2) as the very     
  first thing.
- EI must happen. The BIOS does intrinsic_ei() later (the PROM does intrinsic_ei() in get_floppy_ready() at rom.c:742, just before the first          
  RECALIBRATE). Without EI, the Z80 won't respond to any interrupt.

4. Channel ordering: vector first, then channels are independent

The CTC does not require channels to be initialized in any particular order — Ch0, Ch1, Ch2, Ch3 are independent once the vector base has been set. You
can program Ch3 without touching Ch0-Ch2.

However, the vector base word must be written to Ch0's port before any channel generates an interrupt. Both working implementations write the vector    
base first, then program all channels in order. The ordering of Ch0→Ch1→Ch2→Ch3 is convention, not requirement.
                                                                                                                                                          
---                                                                                                                                                   
Summary: likely root cause

Your description says you write 0x08 to port 0x0C (vector), 0xD7 to port 0x0F (mode), 0x01 to port 0x0F (count). That sequence is correct. Check these:

1. Is IM2 actually active? Did you set the I register and execute IM 2 before enabling the CTC?
2. Is EI executed before the RECALIBRATE? If interrupts are globally disabled, CTC interrupts won't fire.
3. Is the IVT correctly populated at the address (I register << 8) + vector_base + 6? Ch3's vector offset is 6 (channel 3 × 2). If your vector base is  
   0x08, the Z80 will read the ISR address from (I<<8) + 0x0E.
4. Is the IVT page-aligned? The I register provides the high byte. The low byte comes from the CTC. If the IVT isn't where the Z80 expects it, you get a
   jump to garbage.                                
5.  ---                                                                                                                                                     
Disk image format

The PROM uses IMD directly: FLOPPY = $(HOME)/Downloads/SW1711-I8.imd — passed straight to MAME as -flop1. MAME's IMD loader handles the mixed FM/MFM    
per-track format natively. No conversion needed for the PROM.

The BIOS uses MFI (converted from IMD): The BIOS Makefile patches a BIOS binary onto the IMD, then converts to MFI via floptool flopconvert auto mfi.   
MFI is required because IMD is read-only in MAME and the BIOS needs writable disk for SYSGEN etc. But for a clean room reimplementation that only needs
to boot and read, IMD works fine — the PROM boots with it every time.

MAME machine configuration

No special settings beyond the command line. The key flags:

- rc702 for 8" maxi, rc702mini for 5.25" mini
- -bios 0 selects the roa375.ic66 PROM
- -flop1 path.imd (or .mfi)
- No special .cfg overrides needed — just image directories and keyboard enable

The MAME driver (rc702.cpp) sets the FDC data rate in machine_reset() based on DIP switch bit 7: 500 kbps for 8" maxi, 250 kbps for 5.25" mini. This is
automatic per machine variant.

The likely root cause: motor control and drive ready

This is almost certainly the issue. Here's what the working code does:

PROM boot sequence (works in MAME)

1. init_fdc(): 391ms delay, poll MSR until bits 4:0 all clear, send SPECIFY (0x03, 0x4F, 0x20)
2. get_floppy_ready(): read SW1 bit 7, intrinsic_ei(), motor(1) (writes 0x01 to port 0x14)
3. boot_from_floppy_or_jump_prom1(): 391ms motor spin-up delay, then SENSE DRIVE STATUS, check RDY bit in ST3, then RECALIBRATE

BIOS cold boot sequence (works in MAME)

1. Full hardware init: IVT, PIO, CTC (all 4 channels), SIO, DMA, CRT, FDC
2. FDC init: SPECIFY command from CONFI block (0x03, SRT/HUT, HLT/DMA)
3. fdstar() before first disk access: if mini (SW1 bit 7 set), write 0x01 to port 0x14, then wait 1 second (50 × 20ms ticks). If maxi (bit 7 clear),    
   skip motor control entirely — 8" drives have always-on motors.

What the other instance might be missing

For 8" maxi (rc702): Motor is always on. But the MAME floppy drive's ready signal depends on:
- A disk image being mounted (-flop1)
- The drive being "connected" in the MAME driver

MAME's floppy subsystem only asserts the READY line on the floppy connector when the motor is running (for drives that have software motor control) or  
when the drive type says "always ready." The rc702 variant uses 8" drives which in the MAME driver are defined as always-spinning. But if the other     
instance is using rc702mini, the drive has software motor control, and READY won't assert until port 0x14 bit 0 is written as 1.

Critical: the µPD765 RECALIBRATE command will not complete (no INT) if the drive reports "not ready." The FDC sets ST0.NR (Not Ready, bit 3) and raises
INT immediately, but ST0 will be 0x48 (Abnormal Termination + NR) instead of 0x20 (Seek End). If the ISR does SENSE INTERRUPT and the mainline checks
for ST0 == 0x20 | drive, it will see a failure.

But there's a subtler issue: if the FDC never sees the drive as ready at all, it might not even assert INT. In that case, the CTC Ch3 trigger never     
fires, and you hang forever waiting for the interrupt flag.

Checklist for the other instance

1. Are you sending SPECIFY before RECALIBRATE? The SPECIFY command (0x03) sets DMA mode and timing. Without it, the FDC may behave unpredictably. Both  
   PROM and BIOS send SPECIFY early in init.
2. Are you controlling the motor? For rc702mini: write 0x01 to port 0x14, then wait ~1 second before RECALIBRATE. For rc702: motor is always on, no     
   action needed — but verify you're using the rc702 machine variant.
3. Which machine variant? If running rc702 (maxi), motor control is irrelevant. If running rc702mini, you must turn on the motor and wait for spin-up.
4. Are you checking drive ready before RECALIBRATE? The PROM does SENSE DRIVE STATUS (0x04) and checks the RDY bit (bit 5 of ST3) before issuing        
   RECALIBRATE. If the drive isn't ready, it doesn't try.
5. Is there a disk image loaded? Without -flop1, the FDC will never see a ready drive.
6. Have you applied the ST0 HD bit fix? The MAME UPD765 regression (commit 272ec75ca61) causes wrong ST0 values after seek/recalibrate. Without the fix
   (command[1] & 7 → command[1] & 3 in upd765.cpp), ST0 after RECALIBRATE may not match what your BIOS expects.                                            
                                
---
---                                                                                                                                                     
The daisy chain is in hardware AND in MAME software — and this is almost certainly your problem.

MAME daisy chain (hardcoded in rc702.cpp)

static const z80_daisy_config daisy_chain_intf[] = {                                                                                                  
{ "ctc1" },    // CTC1 — highest priority                                                                                                           
{ "sio1" },    // SIO  — middle priority                                                                                                            
{ "pio" },     // PIO  — lowest priority                                                                                                          
{ nullptr }                                                                                                                                         
};

This is a MAME-side configuration that mirrors the physical daisy chain wiring on the RC702 PCB. The Z80 peripheral IEI/IEO pins are wired: CPU → CTC1 →
SIO → PIO. You cannot change this from Z80 software — it's hardware.

In MAME's Z80 daisy chain emulation, each device in the chain must properly manage its interrupt state (IEI in, IEO out) for lower-priority devices'    
interrupts to propagate and for the Z80 to acknowledge them.

The working BIOS does NOT do anything special in software for the daisy chain

There is no "daisy chain setup" code. The daisy chain is entirely hardware. What the BIOS does is:

1. Initialize PIO before CTC — writes interrupt vectors (0x20, 0x22), sets modes, enables interrupts (bios_hw_init.c:140-150)
2. Initialize CTC — writes vector base 0x00, then programs all 4 channels (bios_hw_init.c:153-161)
3. Initialize SIO — channel reset (WR0=0x18), then vector, modes, and crucially WR1 which enables interrupts (bios_hw_init.c:165-187)
4. Read SIO status registers to clear any pending conditions (bios_hw_init.c:190-195)

The PROM does the same order: PIO first, then CTC, then nothing else (no SIO).

But here's the critical insight about CTC Ch.2 vs Ch.3

You say Ch.2 (display) works but Ch.3 (floppy) doesn't. Within the CTC, channels share the same daisy chain device. The CTC's internal priority is Ch0 >
Ch1 > Ch2 > Ch3. If Ch.2 has a pending interrupt that hasn't been properly acknowledged with RETI, it blocks Ch.3.

This is the most likely cause. In the Z80 daisy chain (and inside the CTC), an interrupt is "pending" or "in service" until the CPU executes RETI. The  
Z80 peripherals decode the RETI instruction on the bus. If a higher-priority interrupt is in service (IEO held low), lower-priority interrupts are    
blocked.

Check your display ISR: does it end with RETI (the actual Z80 opcode ED 4D), not just RET (C9)? A plain RET will return from the interrupt but the CTC  
will never see the RETI bus cycle, so Ch.2 remains "in service" and permanently blocks Ch.3.

This is a classic Z80 bug: RET vs RETI. They both return, so the ISR appears to work — the display keeps refreshing because Ch.2 fires again after each
RETI from the previous Ch.2 interrupt. But Ch.3 is permanently starved.

How the working BIOS's ISRs handle this

SDCC's __interrupt attribute generates RETI (not RET) at the end of the function. The __critical __interrupt variant adds DI at entry and EI; RETI at   
exit.

SDCC's __naked ISRs (like isr_crt and isr_floppy) handle it manually — they use helper macros isr_enter_full() / isr_exit_full() which save/restore     
registers and end with EI; RETI.

The PROM's ISRs use __critical __interrupt(2) which SDCC compiles to end with EI; RETI.

The original asm PROM's DUMINT daisy chain clearing

The original ROA375 assembly PROM calls DUMINT early in boot — this is EI; RETI. The comment says "CLEAR DAISY CHAIN". By executing EI followed by RETI
with no interrupt actually pending, it clears any leftover interrupt-in-service state from the daisy chain. The C reimplementation in rom.c doesn't have
this explicit call because the CTC is freshly initialized, but it's worth noting.

Summary: what to check

1. Every ISR must end with RETI (ED 4D), not RET (C9). If your CTC Ch.2 display ISR uses RET, it will block Ch.3 forever. Disassemble your ISR and check
   the last instruction.
2. If using C with a compiler, verify the compiler emits RETI for interrupt handlers. SDCC's __interrupt does this. If your compiler uses               
   __attribute__((interrupt)), check what it actually emits.
3. Consider adding a DUMINT-style daisy chain clear (EI; RETI) before enabling CTC interrupts, just like the original PROM does. This clears any stale
   in-service state.
4. Your IVT arithmetic should be: I=0xF6, CTC vector base=0x08, Ch.3 vector = 0x08 + 6 = 0x0E, IVT address = 0xF60E. Verify that 0xF60E contains a valid
   pointer to your floppy ISR.

4. Wait — you said CTC vector base 0x08. The working BIOS uses vector base 0x00. With base 0x08, Ch.3 = 0x0E. Make sure your IVT has the floppy ISR at  
   byte offset 0x0E from the IVT start (the 8th word entry, index 7).                                 