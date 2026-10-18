# Giga Wing — the port's progress

What is known about the game's memory, and **how** each thing was found. The rule is the toolkit's:
no data from the game is kept here, only addresses and the method that found them. The ROM set is
never committed.

## The ROM set

This port is made from MAME's `gigawing` set, **Giga Wing (USA 990222)**. The set was told apart from
the clones by its files' CRC-32s, listed in [`game.cfg`](game.cfg) and matching the `gigawing` entry in
MAME's CPS-2 driver (`src/mame/capcom/cps2.cpp`):

| Part | Files | Notes |
| --- | --- | --- |
| program (68000) | `ggwu.03`, `ggwu.04`, `ggw.05` | 512 KiB each; the first 1.5 MiB of the board's 4 MiB code region |
| graphics | `ggw.13m`, `ggw.15m`, `ggw.17m`, `ggw.19m` | 4 MiB each, four to a 64-bit word (16 MiB) |
| sound_program (Z80) | `ggw.01` | 128 KiB; the first 32 KiB are mapped, the rest is banked |
| samples (QSound) | `ggw.11m`, `ggw.12m` | 4 MiB each (8 MiB) |
| key | `gigawing.key` | read only, used on the fly as the board does |

`code_end = 180000`: the program ROMs fill the first 1.5 MiB of the board's 4 MiB code region, so the
translator does not look past that.

## The boot self-test, and the part of the code region with no ROMs

The port crashed before it drew its second picture: an access violation (`0xC0000005`) at frame 3, in every
of `--cpu native`, `--cpu blocks` and `--cpu interp` — so not a mistake of the translator's. The toolkit's
tools found it, and the fix is one hook in the game's own sources.

**Finding it.** The port was built again with debugging information beside the release one (Ninja,
`RelWithDebInfo`) and run under gdb. The crash was in `Machine::Read8` (`machine/machine.cpp`), reading
`m_roms.program[...]` at **0x200FFF**, called from the translated code of the reset routine (`fn_0000E2`,
`work/gigawing/gen/native_000.cpp`, the block at 0x23E). `Read16` and `Read8` map the whole of
0x000000–0x3FFFFF to the program the port loaded and **do not check its length**; `romset.cpp` puts the
ROM files' bytes together with nothing after them, so the byte past the last ROM is whatever follows in
memory. This set's ROMs fill only 0x180000 of the board's 4 MiB code region, which is where 0x200FFF sits.

**What the code is.** The reset routine (the vector at 0x0 leads to 0xE2) sets up the board's I/O, then at
0x234–0x266 walks the *whole* of the code region: eight windows of 0x80000 starting at 0xa2 = 0, adding up
one byte every 0xFFF of each window (an odd stride, so that the walk covers all of every ROM rather than
the same columns of each), putting the eight 16-bit sums at 0xFF0000 (a3 = a5 − 0x8000, a5 = 0xFF8000).
Then `bsr $42E` (0x26A): that routine (a) tests that the work RAM, the video RAM and the object RAM take
what is written to them, by hanging at 0x4EA if they do not (`dbra` around a write/read-back/as-restore
loop), (b) **waits at 0x474–0x482 for the sound program** to answer it — the Z80's `ld a,$77` /
`ld ($CFFF),a` at 0x0006 of `ggw.01`, read by the 68000 as `cmpi.b #$77,$619FFF` — and (c) writes the
68000's side of the handshake (0x88 at 0x619FFB, 0xFF at 0x61801F and 0x619FFF) before returning. The
eight sums are never read back: nothing in the translated code addresses 0xFF0000 as a buffer (the
`−0x7FF0(a5)` counter at 0xFF0010 is a different one). So the sums themselves cost the game nothing; the
walk and the handshake are what matter.

**The fix.** `games/gigawing/src/game.cpp` hooks 0x23E (`AddHook`). The hook does the walk over the ROMs
the port has, reading **0x00 past them** as the board does (the board reads zero from the part of the
region its ROMs do not fill: MAME clears a region of this size — `romload.cpp`, a region with no
`ROMREGION_ERASEFF` and no more than 0x400000 bytes is `memset` with 0 — and the port's own data path
already reads the ROM as it is, ciphered, for the game). In the last five windows everything read is past
the ROMs, so they sum 0, as does the last byte read. The hook charges the board's cycles for the loop
instruction by instruction, leaves the registers as the loop leaves them, and then does the `bsr`: it puts
0x26E on the stack and leaves the 68000 **at 0x42E**, so that the *run loop* runs that routine. It has to
be the run loop: 0x42E waits for the Z80, and only the run loop hands time to the sound side between the
68000's blocks; `Call` (lift.h) runs a routine to its return with no time of its own, and the port hung
inside it (the 68000 spinning at 0x47A forever, the Z80 never started).

**Confirmation.** 3000 frames of `--headless` (about 830 frames a second) and then the same with
`--auto 1:300:1`: the CAPCOM logo (frame 900), the title with "INSERT COIN" (2700), the attract show's
demo, and then a real game — "CREDITS 2", a score of 199370 and hit pop-ups — played by the made-up input.

## The game's own memory

Nothing is found yet. Each entry here will name the address, what it is, and how it was found
(`--dump-ram` + `tools/ramfind.py`, `--watch` against `work/gigawing/program.lst`, or `--trace`). The
first things to find are the credits, the players' lives and bombs, and the score — see
[ROADMAP.md](../../ROADMAP.md).
