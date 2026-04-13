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