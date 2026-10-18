# Giga Wing — features and roadmap

Where this port stands against a finished one, and what each missing piece needs. The reference is the
toolkit's own complete example, [vampire-savior-recomp](https://github.com/tmcleroy/vampire-savior-recomp)
(`games/vampire_savior/src/`), and the toolkit's documents `docs/FEATURES.md` and `docs/READABLE.md`.

Everything below is an **optional feature**: with every feature off, the port must run exactly as the
board does. Nothing from the game (no ROM, no RAM dump, no ripped art/text) may ever be committed — each
thing found is recorded in `PROGRESS.md` as *how* it was found (a RAM diff, a write watch, a trace),
never as the data itself.

## What works today

From the toolkit, with no game work:

- The **launcher**: chooses the ROM set, checks every file by CRC-32, sets the controls, Play.
- The **in-game menu** (Esc, `F1`, a stick's click, or Start+Select): save states (9 slots), Reset,
  Insert coin, Player 2 joins, Quit game.
- **Stop, step, slow** (P, O, `-`/`=`).
- **Run-ahead** and the **Display** options (scaling, picture shape, window size, full screen).
- Every key and button **remappable**, for each player.

Nothing is built on the game's own memory yet (`games/gigawing/src/`); that is the work below.

## What a finished port adds (this is the work)

### 0. Foundation — know the game's memory

The credits, the players' lives and bombs, the score, the stage number and the two players' records
(place, hit box, invulnerability) have to be found and named in `games/gigawing/src/gigawing.h`, with
how each was found in `PROGRESS.md`. Everything below builds on this.

How the toolkit finds these (all in `docs/FEATURES.md`):

- `--dump-ram F:FILE` + `tools/ramfind.py DUMP=VALUE ...` — addresses that held a known value across
  several dumps.
- `--trace LOW-HIGH:FILE` with a run that does known things at known frames.
- `--watch LOW[-HIGH]` — every write to an address and the instruction that made it, read against
  `work/gigawing/program.lst`.
- `--poke ADDR=BYTE,...` to try a guess; `--menu FRAME:KEYS` to press the menu's own rows;
  `--shot-every N:PFX` to see a run.

### 1. The Features page (the game's own switches)

| Feature | Kind | Needs |
| --- | --- | --- |
| `free_play` | on/off | the credit word, so Start works with no coin |
| `infinite_lives` | on/off | the players' lives |
| `infinite_bombs` | on/off | the REFLECT/BOMB count |
| `infinite_shield` | on/off | the shield timer |
| `no_damage` | on/off | the players' invulnerability flag, or their hit box |
| `stage_select` | choice | the stage number the game reads before a run starts |
| Board/operator settings (difficulty, lives, free play) | choices | the EEPROM's operator settings |
| "The game's own test menu" | command | a routine address to jump to |

### 2. Practice and replay aids

| Piece | What it adds | Needs |
| --- | --- | --- |
| Input display | what each player holds, and for how long | the players' input words |
| Bullet count / slowdown meter | what the board is being asked to draw | the object lists, or the video's own counts |
| Stage practice | start at a chosen stage, with a chosen power level | the stage number and the run state |
| Autofire | a held button fires at a chosen rate | the input words |

### 3. Alternative modes

Each is a command with a page of its own: a **demo watcher** (the credit word held at zero), and a
**marathon** (the run state held so the game never ends). The board's two-player mode is already there
(Insert coin/Player 2).

### 4. Widescreen

A switch on the launcher's first page, off in every preset (it can change how the game plays). The
field is shown in 16:9, with more of the playfield width visible, at the same height and scale. Needs
rewrites of the camera/scroll registers and of the objects that must be kept while they are in the wide
picture. The toolkit's `widescreen.cpp` in Vampire Savior is the example.

### 5. Readable rewrites (correctness, and what the above builds on)

Rewrite the game's key routines by hand as named C++ (`docs/READABLE.md`): the players' movement and
collision, the reflect, the enemy and bullet spawners, and the score. This is what makes clean cheats
and the practice aids possible, and it is the part most worth doing carefully.

## Recommended order

1. **The foundation** — the credits, the lives/bombs and the players' records.
2. **The Features page** — free play and the cheats, small and self-contained.
3. **Practice and replay aids** — the input display first, then stage practice and autofire.
4. **Readable rewrites** — needed by 3 and 5, and correctness.
5. **Widescreen.**
6. **Alternative modes.**

## Before uploading: a checklist

- [x] `README.md` states the exact ROM set (`gigawing`, USA 990222) and that **only that revision**
      works, with the Capcom disclaimer.
- [x] `rom/`, `build/`, `work/` are ignored and **absent** from the repository; no game data, keys or
      dumps anywhere (`.github/workflows/check.yml` fails a push that tracks one); no screenshots.
- [x] `PROGRESS.md` records how each finding was made (a RAM diff, a watch, a trace), not the data.
- [x] `make check-toolkit`, `make check-common` pass (the CI runs these).
- [x] `LICENSE` (GPL-3.0-only) and `THIRD_PARTY_NOTICES.md` (the cipher is MAME's, BSD-3-Clause); the
      release archives carry them and the licences of what they ship.
- [ ] `make check` passes with a ROM set in place (needs the set).
- [ ] The repository builds from a fresh clone with the player's own ROM set (`make`, `make explore`,
      `make launcher`).
