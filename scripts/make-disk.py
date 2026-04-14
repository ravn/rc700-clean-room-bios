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

    The PROM loads all of track 0 into memory:
      Head 0: sectors 1-26 at 128B each → 0x0000-0x0CFF
      Head 1: sectors 1-26 at 256B each → 0x0D00-0x26FF
    The stub at 0x0280 (sector 6) copies both head 0 and head 1
    payload to 0xC000, then jumps there.

    Machine code (32 bytes):
      F3           DI
      ; Copy head 0 payload (sectors 6-26, after stub)
      21 xx xx     LD HL, src1
      11 00 C0     LD DE, 0xC000
      01 xx xx     LD BC, count1
      ED B0        LDIR
      ; Copy head 1 payload (sectors 1-26 at 0x0D00)
      21 00 0D     LD HL, 0x0D00
      ; DE already points to continuation address
      01 xx xx     LD BC, count2
      ED B0        LDIR
      31 00 F5     LD SP, 0xF500
      FB           EI
      C3 00 C0     JP 0xC000
    """
    STUB_LEN = 32
    HEAD0_MAX = 2688  # sectors 6-26 head 0, 21 x 128 bytes
    HEAD1_MAX = 6656  # sectors 1-26 head 1, 26 x 256 bytes
    head0_data_size = min(bios_size, HEAD0_MAX - STUB_LEN)
    remaining = bios_size - head0_data_size
    head1_data_size = min(max(remaining, 0), HEAD1_MAX)
    src1_addr = 0x0280 + STUB_LEN

    stub = bytearray([
        0xF3,                                           # DI
        # Copy head 0 payload
        0x21, src1_addr & 0xFF, (src1_addr >> 8) & 0xFF, # LD HL, src1
        0x11, 0x00, 0xC0,                               # LD DE, 0xC000
        0x01, head0_data_size & 0xFF, (head0_data_size >> 8) & 0xFF, # LD BC, count1
        0xED, 0xB0,                                     # LDIR
        # Copy head 1 payload
        0x21, 0x00, 0x0D,                               # LD HL, 0x0D00
        # DE is already at 0xC000 + head0_data_size
        0x01, head1_data_size & 0xFF, (head1_data_size >> 8) & 0xFF, # LD BC, count2
        0xED, 0xB0,                                     # LDIR
        # Jump to BIOS
        0x31, 0x00, 0xF5,                               # LD SP, 0xF500
        0xFB,                                           # EI
        0xC3, 0x00, 0xC0,                               # JP 0xC000
    ])
    # Pad to STUB_LEN
    while len(stub) < STUB_LEN:
        stub.append(0x00)
    assert len(stub) == STUB_LEN
    total = head0_data_size + head1_data_size
    print(f"  Loader: {STUB_LEN} bytes, head0: {head0_data_size}B + head1: {head1_data_size}B = {total}B")
    return bytes(stub)


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

    # Inject CCP+BDOS onto track 1 (if binary exists)
    ccp_bdos_path = os.path.join(os.path.dirname(bios_file), '..', 'cpm', 'ccp_bdos.bin')
    if not os.path.exists(ccp_bdos_path):
        ccp_bdos_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'cpm', 'ccp_bdos.bin')
    if os.path.exists(ccp_bdos_path):
        with open(ccp_bdos_path, 'rb') as f:
            ccp_data = f.read()
        # Write CCP+BDOS to track 1 head 0 using the 8" DD MFM interleave.
        # The BIOS reads host sectors 0,1,2,... which translate via tran8
        # to physical sectors 1,5,9,13,2,6,10,14,3,7,11,15,4,8,12.
        # So host sector 0 (first 512B of CCP) goes to physical sector 1,
        # host sector 1 (next 512B) goes to physical sector 5, etc.
        tran8 = [1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12]
        for t in tracks:
            if t['cyl'] == 1 and t['head'] == 0:
                for host_sec in range(len(tran8)):
                    offset = host_sec * t['secsize']
                    if offset >= len(ccp_data):
                        break
                    phys_sec = tran8[host_sec]
                    chunk = ccp_data[offset:offset + t['secsize']]
                    if len(chunk) < t['secsize']:
                        chunk = chunk + bytes(t['secsize'] - len(chunk))
                    t['sectors'][phys_sec] = chunk
                secs_written = min(len(tran8), (len(ccp_data) + t['secsize'] - 1) // t['secsize'])
                print(f"  CCP+BDOS: {len(ccp_data)} bytes interleaved to track 1 ({secs_written} sectors)")
                break
    else:
        print(f"  WARNING: CCP+BDOS not found at {ccp_bdos_path}")

    # Rebuild and write
    output_data = rebuild_imd(header, tracks)
    with open(output_imd, 'wb') as f:
        f.write(output_data)
    print(f"Output IMD: {len(output_data)} bytes -> {output_imd}")


if __name__ == '__main__':
    main()
