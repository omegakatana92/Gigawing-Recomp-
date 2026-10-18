// SPDX-License-Identifier: GPL-3.0-only
//
// Giga Wing (USA 990222): what the game's own sources add to the port (see PROGRESS.md).
//
// One hook, for the self-test the board runs before it draws anything. Nothing here is the game's data:
// only where the game keeps a thing, and what the board does with the part of its program region that has
// no ROMs in it.
#include "game.h"
#include "lift.h"

namespace cps2recomp {
namespace {

// The board's 68000 program region is 4 MiB (the toolkit's CODE_SIZE), and the boot self-test walks all of
// it: eight windows of 0x80000, adding up one byte every 0xFFF of each (an odd stride, so that a window
// covers the whole of every chip rather than the same columns of each), and putting the eight 16-bit sums
// at 0xFF0000. Past the ROMs a window sums 0x00: the board reads zero from the part of the region its ROMs
// do not fill (MAME clears a region of this size, romload.cpp), and this set's ROMs leave that part empty,
// too: ggwu.03, ggwu.04 and ggw.05 fill only the first 0x180000 (games/gigawing/game.cfg). The port holds
// only what the ROMs fill, so the walk runs off the end of what it loaded (an access violation at
// 0x200FFF, the fourth window's second byte) and cannot run as the game wrote it. The hook walks what
// there is, 0x00 past it, and then leaves the 68000 as the loop and the bsr after it would have: the same
// registers, the same sums in memory, and the return address the bsr pushes at 0x42E, where the run loop
// takes over (see below). The game keeps the sums and never reads them back (nothing addresses 0xFF0000 as
// a buffer), so what has to be right is the walk, the registers and the sound handshake the bsr's routine
// does. PROGRESS.md tells in full how this was found.
constexpr uint32_t kWindowSums = 0x23E;     // (the block the self-test's loop starts at)
constexpr uint32_t kWindowSumsDone = 0x26E; // (where the loop and the bsr leave the 68000)
constexpr uint32_t kSelfTest = 0x42E;       // (the routine the bsr at 0x26A calls, and where the hook leaves the CPU)
constexpr uint32_t kWindow = 0x80000;       // (a window: the region the board's ROMs cover, eight times over)
constexpr uint32_t kStride = 0xFFF;         // (the step of the walk)

// What the 68000 reads from the program region: the ROMs where this set has them, 0x00 past them
uint8_t ProgramRead(M68K &c, uint32_t address) {
    return address < c.romSize ? c.rom[address] : 0x00;
}

// The hook is handed the 68000 at 0x23E, with a2 at the first window, a3 at 0xFF0000 (where the sums go)
// and d6 counting the eight windows. It does the walk and the bsr, and hands the 68000 to the run loop at
// 0x42E, which is a routine of the game's own: that routine is where the game waits for the sound program
// to answer it (the Z80's 0x77 at 0x619FFF), and only the run loop can let the Z80 run meanwhile. Call
// (lift.h) may not be used for it: it runs a routine to its return with no time of its own, and this one
// waits for another CPU.
bool WindowSums(M68K &c) {
    uint32_t window = c.a[2];         // (the window the walk is at: 0, 0x80000, ... as the game set it up)
    uint32_t sums = c.a[3];           // (where the sums go: 0xFF0000, two bytes a window)
    uint32_t count = c.d[6] & 0xFFFF; // (the dbra's counter: seven, for the eight windows)
    uint32_t cycles = 0;
    uint32_t sum = 0, last = 0, at = window, end = window;
    for (;;) {
        // 0x23E-0x24A: the window's sum is 0, the walk starts at the window and ends a window on
        sum = 0;
        at = window;
        end = window + kWindow;
        cycles += 4 + 4 + 4 + 14 + 14; // moveq d0; lea (a2),a0; lea (a0),a1; adda.l #$80000,a1; cmpi.l (a stray compare)
        // 0x250-0x25C: a byte every 0xFFF, added as the 68000 adds (a 32-bit sum, wrapping, d1 zero-extended),
        // while the address is below the window's end (bcs: it borrows for as long as it is)
        for (; at < end; at += kStride) {
            last = ProgramRead(c, at);
            sum += last;
            const bool more = at + kStride < end;
            cycles += 4 + 8 + 6 + 8 + 6 + (more ? 10 : 8); // moveq d1; move.b (a0),d1; add.l d1,d0; lea 0xFFF(a0),a0; cmpa.l a1,a0; bcs
        }
        // 0x25E-0x266: the sum, its low word, to the game's memory; the next window; and the dbra
        c.Write(sums, 1, sum & 0xFFFF);
        sums += 2;
        window += kWindow;
        cycles += 8 + 14; // move.w d0,(a3)+; adda.l #$80000,a2
        const uint32_t left = (count - 1) & 0xFFFF;
        count = left;
        if (left != 0xFFFF) {
            cycles += 10;
            continue;
        }
        cycles += 14;
        break;
    }
    // What the loop leaves behind: the last window's sum and the last byte read (the last five windows are
    // all past the ROMs, so both are 0), the counter, and the addresses the walk stopped at (past the
    // window's end, so the last cmpa.l did not borrow)
    c.d[0] = sum;
    c.d[1] = last;
    c.d[6] = (c.d[6] & 0xFFFF0000) | count;
    c.a[0] = at;
    c.a[1] = end;
    c.a[2] = window;
    c.a[3] = sums;
    c.flagC = c.flagN = c.flagV = c.flagZ = false;
    // 0x26A: the bsr, which the run loop then runs the routine of (the return address goes on the stack as
    // the bsr's own two writes put it)
    cycles += 18;
    c.a[7] -= 4;
    c.Write(c.a[7], 2, kWindowSumsDone);
    Spend(c, cycles);
    c.pc = kSelfTest;
    return true;
}

} // namespace

// (the toolchain reads the addresses out of these sources: it looks for "AddHook(0x")
void RegisterGame() {
    AddHook(kWindowSums, "WindowSums", WindowSums);
}

} // namespace cps2recomp
