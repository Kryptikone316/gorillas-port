# Gorillas

A new SDL2 game in the spirit of the QBasic *Gorillas* (Microsoft, 1990): two gorillas on the roofs of
a city take turns throwing exploding bananas at each other. Set the angle and velocity, allow for the
wind and the gravity, and blow holes in the skyline. It contains no code, pictures or sounds from the
original: the game, the art and the sounds are all written for this project (art is drawn in code,
sounds are synthesised at start-up). Not affiliated with or endorsed by Microsoft.

Made for the Data Frog SF3500 (TreeFrogUI Ports Collection), and it also builds and runs on a desktop.

* Play against five computer opponents (Cub to Silverback), two players on one pad, or watch two
  computers play. Ten worlds' gravity, wind off / per round / every throw, 1 to 15 rounds, Quick Game.
* Five themes, purely cosmetic: **Q-BASIC** (the original's flat look, text-screen menu and
  PC-speaker-style sounds), **Classic**, **Uptown**, **Moonlit** and **Harbor**. Twelve gorilla looks.
* On-screen number keypad and keyboard for the pad. Nothing in the game flashes or animates beyond play itself.

## Building

* Desktop (needs SDL2): `./build_host.sh` -> `build-host/gorillas` (keyboard: arrows, Z = A, X = B, C = X,
  Enter = Start, Esc quits).
* SF3500 (needs the buildroot MIPS toolchain, see the header of `build_sf3500.sh`): `./build_sf3500.sh`,
  then `./package.sh` assembles `package/`.
* `DEV-NOTES.md` lists the test hooks (`GOR_*` environment variables) and how the bot, the themes and the
  platform layer fit together. `tests/shotcmp.c` checks the shot-flight shortcut against plain stepping.

## Licence

MIT, see `LICENSE`. Third-party parts: `src/font8x8.h` (public domain, Daniel Hepper / Marcel Sondaar);
the display, audio and pad glue in `plat/` is derived from TreeFrogUI's pico286 SF3000 patch and is
distributed here under the same MIT licence.
