#!/usr/bin/env python3
"""
Create a bootable RC702 disk image with our BIOS on track 0.

Takes the original SW1711 IMD image and replaces the BIOS sectors
on track 0 with our compiled binary. The boot sector (sector 1),
CONFI block (sector 2), and character tables (sectors 3-5) are
preserved from the original.

Track 0 layout:
  Head 0 (FM, 26 sectors x 128 bytes):
    Sector 1:    Boot sector (start address + machine ID) — preserved
    Sector 2:    CONFI configuration block — preserved
    Sectors 3-5: Character conversion tables — preserved
    Sectors 6-26: BIOS code part 1 (21 x 128 = 2688 bytes)
  Head 1 (MFM, 26 sectors x 256 bytes):
    Sectors 1-26: BIOS code part 2 (26 x 256 = 6656 bytes)

Total BIOS space: 2688 + 6656 = 9344 bytes

Usage: make-disk.py <bios_binary> <source_imd> <output_imd>
"""

import sys
import struct
import os

def parse_imd(data):
    """Parse IMD file into header + list of track records."""
    hdr_end = data.index(0x1A)
    header = data[:hdr_end + 1]
    pos = hdr_end + 1
    tracks = []

    while pos < len(data):
        mode = data[pos]
        cyl = data[pos + 1]
        head = data[pos + 2] & 0x01
        nsec = data[pos + 3]
        secsize_code = data[pos + 4]
        secsize = 128 << secsize_code
        track_start = pos
        pos += 5

        # Sector map
        secmap = list(data[pos:pos + nsec])
        pos += nsec

        # Sector data
        sectors = {}
        for i in range(nsec):
            rec_type = data[pos]
            pos += 1
            if rec_type == 0:
                sectors[secmap[i]] = bytes(secsize)
            elif rec_type == 1:
                sectors[secmap[i]] = bytes(data[pos:pos + secsize])
                pos += secsize
            elif rec_type == 2:
                fill = data[pos]
                pos += 1
                sectors[secmap[i]] = bytes([fill] * secsize)
            elif rec_type == 3:
                sectors[secmap[i]] = bytes(data[pos:pos + secsize])
                pos += secsize
            elif rec_type == 4:
                fill = data[pos]
                pos += 1
                sectors[secmap[i]] = bytes([fill] * secsize)
            else:
                if rec_type & 1:
                    sectors[secmap[i]] = bytes(data[pos:pos + secsize])
                    pos += secsize
                else:
                    fill = data[pos]
                    pos += 1
                    sectors[secmap[i]] = bytes([fill] * secsize)

        tracks.append({
            'mode': mode, 'cyl': cyl, 'head': head,
            'nsec': nsec, 'secsize': secsize, 'secsize_code': secsize_code,
            'secmap': secmap, 'sectors': sectors,
            'raw_start': track_start
        })

    return header, tracks


def rebuild_imd(header, tracks):
    """Rebuild IMD file from header + track records."""
    out = bytearray(header)

    for t in tracks:
        # Track header
        out.append(t['mode'])
        out.append(t['cyl'])
        out.append(t['head'])
        out.append(t['nsec'])
        out.append(t['secsize_code'])

        # Sector map
        out.extend(t['secmap'])

        # Sector data (write as type 1 = normal data)
        for s in t['secmap']:
            data = t['sectors'].get(s, bytes(t['secsize']))
            # Check if sector is uniform (could compress as type 2)
            if len(set(data)) == 1:
                out.append(2)  # compressed
                out.append(data[0])
            else:
                out.append(1)  # normal data
                out.extend(data)

    return bytes(out)


def make_loader_stub(bios_size):
    """Create a Z80 relocator stub that copies BIOS to 0xC000.

    The stub is loaded at 0x0280 by the ROA375 PROM.
    It copies bios_size bytes from (0x0280 + stub_length) to 0xC000,
    then jumps to 0xC000.

    Machine code (15 bytes):
      F3           DI
      21 xx xx     LD HL, source
      11 00 DA     LD DE, 0xC000
      01 xx xx     LD BC, count
      ED B0        LDIR
      C3 00 DA     JP 0xC000
    """
    # Two-stage loader:
    # Stage 1 (in loader): copy head 0 portion to 0xC000
    # Stage 2 (in loader): read head 1 via FDC+DMA, append at 0xC000 + head0_size
    # Stage 3: JP 0xC000
    #
    # Head 0 data available: sectors 6-26 = 2688 bytes
    # Loader code takes ~110 bytes, leaving ~2578 for BIOS part 1
    # Head 1 data: 26 x 256 = 6656 bytes for BIOS part 2
    # Total BIOS: ~9234 bytes

    # Calculate how much BIOS fits in head 0 after the loader
    LOADER_MAX = 2688  # sectors 6-26 of head 0
    HEAD1_SIZE = 26 * 256  # 6656 bytes

    # Build the loader machine code
    # We'll calculate addresses after we know the loader size
    code = bytearray()

    # DI
    code.append(0xF3)

    # Phase 1: LDIR head 0 data to 0xC000
    # LD HL, bios_data_start  (patched below)
    p_ld_hl_src = len(code)
    code.extend([0x21, 0x00, 0x00])  # placeholder
    # LD DE, 0xC000
    code.extend([0x11, 0x00, 0xC0])
    # LD BC, head0_data_size  (patched below)
    p_ld_bc_size = len(code)
    code.extend([0x01, 0x00, 0x00])  # placeholder
    # LDIR
    code.extend([0xED, 0xB0])

    # Phase 2: Read head 1 sectors via FDC
    # LD HL, dest = 0xC000 + head0_data_size  (patched below)
    p_ld_hl_dest = len(code)
    code.extend([0x21, 0x00, 0x00])  # placeholder
    # LD C, 1 (starting sector)
    code.extend([0x0E, 0x01])
    # LD B, 26 (sector count)
    code.extend([0x06, 26])

    # -- sector_loop: --
    sector_loop = len(code)
    # PUSH BC
    code.append(0xC5)
    # PUSH HL
    code.append(0xE5)

    # DMA setup
    code.extend([0x3E, 0x05, 0xD3, 0xFA])  # LD A,5; OUT (DMA_MASK),A  mask ch1
    code.extend([0x3E, 0x45, 0xD3, 0xFB])  # LD A,0x45; OUT (DMA_MODE),A
    code.extend([0xAF, 0xD3, 0xFC])        # XOR A; OUT (DMA_CLEAR),A
    code.extend([0x7D, 0xD3, 0xF2])        # LD A,L; OUT (DMA_ADDR1),A  addr lo
    code.extend([0x7C, 0xD3, 0xF2])        # LD A,H; OUT (DMA_ADDR1),A  addr hi
    code.extend([0x3E, 0xFF, 0xD3, 0xF3])  # LD A,255; OUT (DMA_COUNT1),A  cnt lo
    code.extend([0xAF, 0xD3, 0xF3])        # XOR A; OUT (DMA_COUNT1),A  cnt hi
    code.extend([0x3E, 0x01, 0xD3, 0xFA])  # LD A,1; OUT (DMA_MASK),A  unmask ch1

    # FDC READ DATA command (9 bytes)
    # Need sector number from C (on stack). POP BC, PUSH BC to access.
    code.append(0xE1)  # POP HL (was dest)
    code.append(0xC1)  # POP BC (B=remaining, C=sector)
    code.append(0xC5)  # PUSH BC
    code.append(0xE5)  # PUSH HL

    # Send 9 command bytes
    fdc_out_addr = None  # will be set below

    # LD A, 0x46; CALL fdc_out  (READ DATA MFM)
    code.extend([0x3E, 0x46])
    p_call1 = len(code)
    code.extend([0xCD, 0x00, 0x00])  # placeholder

    # LD A, 0x04; CALL fdc_out  (head=1, drive=0)
    code.extend([0x3E, 0x04])
    p_call2 = len(code)
    code.extend([0xCD, 0x00, 0x00])

    # XOR A; CALL fdc_out  (cylinder=0)
    code.extend([0xAF])
    p_call3 = len(code)
    code.extend([0xCD, 0x00, 0x00])

    # LD A, 1; CALL fdc_out  (head=1)
    code.extend([0x3E, 0x01])
    p_call4 = len(code)
    code.extend([0xCD, 0x00, 0x00])

    # LD A, C; CALL fdc_out  (sector number)
    code.extend([0x79])
    p_call5 = len(code)
    code.extend([0xCD, 0x00, 0x00])

    # LD A, 1; CALL fdc_out  (N=1, 256B sectors)
    code.extend([0x3E, 0x01])
    p_call6 = len(code)
    code.extend([0xCD, 0x00, 0x00])

    # LD A, 26; CALL fdc_out  (EOT)
    code.extend([0x3E, 26])
    p_call7 = len(code)
    code.extend([0xCD, 0x00, 0x00])

    # LD A, 14; CALL fdc_out  (GPL)
    code.extend([0x3E, 14])
    p_call8 = len(code)
    code.extend([0xCD, 0x00, 0x00])

    # LD A, 0xFF; CALL fdc_out  (DTL)
    code.extend([0x3E, 0xFF])
    p_call9 = len(code)
    code.extend([0xCD, 0x00, 0x00])

    # Read 7 result bytes
    p_call_in = []
    for _ in range(7):
        p_call_in.append(len(code))
        code.extend([0xCD, 0x00, 0x00])  # CALL fdc_in

    # Advance: POP HL (dest), POP BC, LD DE,256, ADD HL,DE, INC C, DJNZ
    code.append(0xE1)  # POP HL
    code.append(0xC1)  # POP BC
    code.extend([0x11, 0x00, 0x01])  # LD DE, 256
    code.extend([0x19])              # ADD HL, DE
    code.extend([0x0C])              # INC C
    # DJNZ sector_loop
    offset = sector_loop - (len(code) + 2)
    code.extend([0x10, offset & 0xFF])

    # Set stack pointer for BIOS (Section 13.1: SP = 0xF500)
    code.extend([0x31, 0x00, 0xF5])  # LD SP, 0xF500

    # JP 0xC000
    code.extend([0xC3, 0x00, 0xC0])

    # -- fdc_out subroutine --
    fdc_out_addr = 0x0280 + len(code)
    code.append(0xF5)  # PUSH AF
    fdc_out_wait = len(code)
    code.extend([0xDB, 0x04])        # IN A, (FDC_MSR)
    code.extend([0xE6, 0xC0])        # AND 0xC0
    code.extend([0xFE, 0x80])        # CP 0x80
    offset = fdc_out_wait - (len(code) + 2)
    code.extend([0x20, offset & 0xFF])  # JR NZ, fdc_out_wait
    code.append(0xF1)                # POP AF
    code.extend([0xD3, 0x05])        # OUT (FDC_DATA), A
    code.append(0xC9)                # RET

    # -- fdc_in subroutine --
    fdc_in_addr = 0x0280 + len(code)
    fdc_in_wait = len(code)
    code.extend([0xDB, 0x04])        # IN A, (FDC_MSR)
    code.extend([0xE6, 0xC0])        # AND 0xC0
    code.extend([0xFE, 0xC0])        # CP 0xC0
    offset = fdc_in_wait - (len(code) + 2)
    code.extend([0x20, offset & 0xFF])  # JR NZ, fdc_in_wait
    code.extend([0xDB, 0x05])        # IN A, (FDC_DATA)
    code.append(0xC9)                # RET

    # Now patch all CALL addresses
    for p in [p_call1, p_call2, p_call3, p_call4, p_call5,
              p_call6, p_call7, p_call8, p_call9]:
        code[p + 1] = fdc_out_addr & 0xFF
        code[p + 2] = (fdc_out_addr >> 8) & 0xFF
    for p in p_call_in:
        code[p + 1] = fdc_in_addr & 0xFF
        code[p + 2] = (fdc_in_addr >> 8) & 0xFF

    loader_size = len(code)
    head0_data_size = min(bios_size, LOADER_MAX - loader_size)
    bios_data_start = 0x0280 + loader_size
    dest_for_head1 = 0xC000 + head0_data_size

    # Patch LD HL, bios_data_start
    code[p_ld_hl_src + 1] = bios_data_start & 0xFF
    code[p_ld_hl_src + 2] = (bios_data_start >> 8) & 0xFF
    # Patch LD BC, head0_data_size
    code[p_ld_bc_size + 1] = head0_data_size & 0xFF
    code[p_ld_bc_size + 2] = (head0_data_size >> 8) & 0xFF
    # Patch LD HL, dest for head 1
    code[p_ld_hl_dest + 1] = dest_for_head1 & 0xFF
    code[p_ld_hl_dest + 2] = (dest_for_head1 >> 8) & 0xFF

    print(f"  Loader: {loader_size} bytes, head0 data: {head0_data_size} bytes, head1: {HEAD1_SIZE} bytes")
    return bytes(code)


def inject_bios(bios_data, tracks):
    """Inject loader stub + BIOS binary into track 0 sectors."""
    # Prepend the relocator stub
    stub = make_loader_stub(len(bios_data))
    payload = stub + bios_data
    print(f"  Loader stub: {len(stub)} bytes, payload: {len(payload)} bytes")

    bios_offset = 0
    bios_len = len(payload)
    bios_data = payload

    # Find track 0 head 0 and head 1
    t0h0 = None
    t0h1 = None
    for t in tracks:
        if t['cyl'] == 0 and t['head'] == 0:
            t0h0 = t
        if t['cyl'] == 0 and t['head'] == 1:
            t0h1 = t

    if not t0h0 or not t0h1:
        print("ERROR: Track 0 head 0 or head 1 not found in IMD")
        sys.exit(1)

    # Head 0: write to sectors 6-26 (128 bytes each)
    for sec_num in range(6, 27):
        if sec_num > t0h0['nsec']:
            break
        chunk = bios_data[bios_offset:bios_offset + 128]
        if len(chunk) < 128:
            chunk = chunk + bytes(128 - len(chunk))  # pad with zeros
        t0h0['sectors'][sec_num] = chunk
        bios_offset += 128

    # Head 1: write to sectors 1-26 (256 bytes each)
    for sec_num in range(1, 27):
        if sec_num > t0h1['nsec']:
            break
        if bios_offset >= bios_len:
            # Pad remaining sectors with zeros
            t0h1['sectors'][sec_num] = bytes(256)
        else:
            chunk = bios_data[bios_offset:bios_offset + 256]
            if len(chunk) < 256:
                chunk = chunk + bytes(256 - len(chunk))
            t0h1['sectors'][sec_num] = chunk
            bios_offset += 256

    written = min(bios_offset, bios_len)
    print(f"  Injected {written} bytes of BIOS into track 0")
    if bios_len > 9344:
        print(f"  WARNING: BIOS ({bios_len} bytes) exceeds track 0 capacity (9344)")
    return tracks


def update_boot_sector(tracks, entry_addr):
    """Update boot sector (track 0, head 0, sector 1) with entry address."""
    for t in tracks:
        if t['cyl'] == 0 and t['head'] == 0:
            sec1 = bytearray(t['sectors'][1])
            # Bytes 0-1: entry point address (little-endian)
            sec1[0] = entry_addr & 0xFF
            sec1[1] = (entry_addr >> 8) & 0xFF
            t['sectors'][1] = bytes(sec1)
            print(f"  Boot sector entry point: 0x{entry_addr:04X}")
            return


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <bios_CODE.bin> <source.imd> <output.imd>")
        sys.exit(1)

    bios_file = sys.argv[1]
    source_imd = sys.argv[2]
    output_imd = sys.argv[3]

    # Read BIOS binary
    with open(bios_file, 'rb') as f:
        bios_data = f.read()
    print(f"BIOS binary: {len(bios_data)} bytes")

    # Read source IMD
    with open(source_imd, 'rb') as f:
        imd_data = f.read()
    print(f"Source IMD: {len(imd_data)} bytes")

    header, tracks = parse_imd(imd_data)

    # Inject BIOS
    tracks = inject_bios(bios_data, tracks)

    # Update boot sector entry point to loader stub (0x0280)
    update_boot_sector(tracks, 0x0280)  # loader stub address

    # Rebuild and write
    output_data = rebuild_imd(header, tracks)
    with open(output_imd, 'wb') as f:
        f.write(output_data)
    print(f"Output IMD: {len(output_data)} bytes -> {output_imd}")


if __name__ == '__main__':
    main()
