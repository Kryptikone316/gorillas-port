# Gorillas (SF3500) - dev notes

Fresh C/SDL2 game in the spirit of Microsoft's QBasic Gorillas (1990). NO Microsoft code/art/sound: all art is
drawn in code (src/art.c), all sounds synthesized (src/sound.c), rules reimplemented (physics = same arc shape as the
original: x = x0 + vx*t + 0.5*(wind/5)*t^2, y = y0 + (-vy*t + 0.5*g*t^2)*(H/350), t += 0.1 per step).
The original game is kept outside this repository as a behaviour reference only; nothing of it is shipped.

## Layout
- src/game.c   menus, turn flow (phases PH_*), HUD, config (gorillas.txt), test hooks, bot_test
- src/world.c  skyline, terrain mask, craters, shot physics (shot_step is shared by game and bot)
- src/bot.c    5 levels (Cub..Silverback): search by simulation + error + wind knowledge + learn-from-miss
- src/osk.c    on-screen numeric keypad / alphanumeric keyboard (modelled on Block Dude's)
- src/art.c, sound.c, gfx.c, font8x8.h (public domain)
- src/plat_sdl.c   SF3500 (-DSF3500: hwdisp/joykey/sf3500_mixer in plat/) and desktop SDL2 window
- build_host.sh -> build-host/gorillas;  build_sf3500.sh -> build-sf3500/;  package.sh -> package/

## Test hooks (env)
GOR_HEADLESS=1, GOR_SEED, GOR_SHOTS=<dir> GOR_SHOT_EVERY=<n> (BMP shots), GOR_MAXFRAMES, GOR_JOY="frame:mask,..."
(tools/joy.py builds it from tokens), GOR_MODE/GOR_DIFF/GOR_DIFF1/GOR_ROUNDS/GOR_GRAV/GOR_INPUT, GOR_AUTOSTART=1,
GOR_BOTTEST=<rounds> (bot ladder stats, no drawing), GOR_WAVS=<dir> (dump sounds), GOR_BOXART=1|2 (+ tools/make_boxart.py).
QEMU: flatpak-spawn --host qemu-mipsel-static -L <buildroot sysroot> build-sf3500/gorillas.mipsel (env via --env=).
MIPS and host frames match to 1 level/channel (SDL BMP writer), game logic identical.

## Bot ladder (150 rounds, Earth, classic wind; avg throws per round for same-level duels)
Cub ~14, Chimp ~9, Baboon ~6.4, Orangutan ~3.6, Silverback ~2.1; Silverback beats Orangutan 2:1.
Search = 3000 sims per throw (~30 ms on x86), run 220 per frame so it never stalls a frame.

## Not done / open
- Not device-tested (display, pad, audio, perf of the bot search on the 74kc).
- Licence of our code not decided; no collection entry/PR yet; no NOTICE/CREDITS files beyond README.txt.
- Box art is ours (tools/make_boxart.py); card needs sync-boxart.sh after copying (boxart.png -> .res/Gorillas.png).

## Gravity worlds (2026-11-18)
Ten worlds in game.c k_grav (Mercury 3.7, Venus 8.9, Earth 9.8, Moon 1.6, Mars 3.7, Jupiter 24.8, Saturn 10.4, Uranus 8.9,
Neptune 11.2, Pluto 0.6; Mercury=Mars and Venus=Uranus play identically). gorillas.txt stores the NAME (old numeric saves
0..4 = Earth/Moon/Mars/Jupiter/Pluto are still read). The Sun (274) was left out: a throw cannot reach the other gorilla.
- shot_step skips the invisible part of a flight high above the screen (exact: tests/shotcmp.c compares 30000 random flights
  with -DSHOT_NO_SKIP, identical). It matters on the light worlds.
- Bot: velocity grid finer at the slow end, angle grid 1 deg at the flat ends, fine sweeps when the grid finds no hit,
  prefers the quickest hits (wind push grows with t^2), per-frame search budget ~6 ms on device (fixed 220 sims headless).
- Remaining no-result rounds on Moon/Pluto are gale-wind rounds: wind accel does not scale with gravity, so wind beats
  gravity there (wind off: 0 failures). Possible tweak NOT made (gameplay): scale wind by gravity.
- GOR_BOTDEBUG=1 with GOR_BOTTEST prints every bot throw.

## Themes (2026-11-19)
src/theme.c/.h: Classic (QBasic's flat EGA colours, the default), Uptown (brick city by day), Moonlit (night, clouds, searchlight),
Harbor (night quay, lighthouse, water reflections). `theme=` in gorillas.txt (name). Menu: Audio/Visual > Theme; Sky has a new
"Auto" (7) = the theme's own sun/moon and is the new default. Quick Game rolls a theme and uses Sky Auto.
- Pure cosmetics: own LCG (crnd) for everything the themes draw, the game's rnd() is untouched. GOR_CITYTEST=<n> prints a hash of the
  terrain mask/gorilla spots/wind per city: identical for all 4 themes (checked), plus ms per city (host 1-7 ms).
- Never put two crnd() calls in one expression/argument list: evaluation order differs between x86 gcc and the MIPS gcc
  (caused a MIPS/host frame mismatch once). Declarations with several initialisers are fine.
- Backdrop (silhouettes, clouds, beam, lighthouse, the street/water strip below BASELINE) is painted into W.sky, so craters reveal it;
  the HUD no longer fills the strip, it shows what the theme painted. Buildings are drawn into W.pic (terrain mask untouched).
- Roof ornaments (water tower, chimneys, aerial, spire) are tracked per pixel (g_deco) and vanish with their roof; street furniture
  (trees, lamps, stalls, barrels...) is redrawn after each crater (it stands in front of the wall).
- Concept pictures in Custom/Concepts are design reference only (AI-generated; nothing embedded), as with SkiFree.
- QEMU vs host: all four themes max diff 1 after 1500 frames of CPU-vs-CPU. Test hooks: GOR_THEME=<0..3>, GOR_BOXART=4 (Game Over panel).

## Q-BASIC theme (2026-11-19)
"Classic" became "Q-BASIC" (config still reads "Classic"): the purist theme. Palette/layout measured from the user's screenshot of the
original (Extras/gorilla_thumbnail.png): sky (0,0,170), buildings (170,0,0)/(0,170,170)/(170,170,170), windows (255,255,85)/(85,85,85),
4 x 9 windows on a 10 x 20 grid, peach (255,170,85) gorillas, explosion (255,0,85) from the EGA palette table in the original source.
ART IS OURS (no Microsoft pictures/code/sound): the gorilla (art.c draw_qb_gorilla, LOOK_QB = 12) is drawn from rectangles on a 32x29 grid
stretched to 40x48; the sun (sky 6, "Q-BASIC Sun", SKY_QB), the flat banana (spr_ban_qb), the PC-speaker sound bank (sound.c
snd_use_speaker, own tunes), the text-mode menu with a flashing '*' border (draw_title_qb, sparkle), the corner HUD and score box
(draw_hud_qb). Under this theme: gorilla/sun forced (rows hidden), no text shadows (gfx_shadows), black bar under the city.
gorillas.txt: sky is stored by NAME now (old numbers still read: 0..5 same, 6 = Random, 7 = Auto). NLOOKS 13, NSKY 7.
If pixel-exact original sprites are ever wanted: they would have to come from the player's own GORILLA.BAS at run time (not shipped).

## Classic is back; nothing flashes (2026-11-19)
Themes: Q-BASIC, Classic (the port's first look: gradient sky, outlined bevelled blocks, our gorillas/suns, old HUD), Uptown, Moonlit, Harbor
(NTHEME 5; "Classic" in an old file now means this one). The Q-BASIC text screens' asterisk border is STEADY (the user removed the
flash option entirely: photosensitivity, and simpler); the title-screen gorillas are still too (arms up, facing each other).
In-game effects (banana, explosions, dance) are gameplay and remain. Old `flash=` lines are ignored.
k_page_rows is [3][14]: keep it larger than the longest page + 1 (an overflow once printed another page's rows).
