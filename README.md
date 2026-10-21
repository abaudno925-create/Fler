<div align="center">

# Fler

**External X11 overlay for Counter-Strike 2 on Linux.**

A fork of [Doctor-Coomer/CounterStrike2-Linux-Cheat](https://github.com/Doctor-Coomer/CounterStrike2-Linux-Cheat)
with a rebuilt, premium Qt menu — dark surfaces, a live accent theme, and settings that
survive restarts.

</div>

## Menu

| Page | What it holds |
| --- | --- |
| **ESP** | Master, team filter, spotted filter, box, health bar/text, name, weapon, skeleton, head dot, cat ears, snap lines — each with its own colour |
| **Aimbot** | Targeting filters, FOV slider, FOV circle, snap lines, recoil control, auto shoot, rebindable aim key |
| **Visuals** | Crosshair with colour, sniper-only crosshair, recoil visualisation, FPS counter |
| **Theme** | Live accent colour with presets, reset to defaults, version info |

![Fler menu — ESP](docs/menu-esp.png)
![Fler menu — Aimbot](docs/menu-aimbot.png)
![Fler menu — Visuals](docs/menu-visuals.png)
![Fler menu — Theme](docs/menu-theme.png)

### What changed against the original menu

- **Frameless window** with rounded corners, a draggable header and a sidebar navigation rail
  instead of a `QTabWidget`.
- **Custom widgets**: animated toggle switches, round colour swatches that open a styled
  picker, a single-key capture button for the aim hotkey, and hand-drawn vector icons.
- **Live accent theme** — pick any colour and the whole menu repaints instantly; six presets
  ship in the box.
- **Persistence** — every option is written to `~/.config/Fler/Fler.conf` and restored on the
  next launch. `Reset to defaults` wipes it back to stock.
- **Qt lives in the main thread** — the original ran `QApplication` on a worker thread, which
  is unsupported; the overlay and game loops are the ones on workers now.

## Build

### Dependencies

Debian/Ubuntu:

```console
$ sudo apt install mesa-common-dev libx11-dev libxfixes-dev libxcomposite-dev gcc g++ make qt6-base-dev
```

Arch Linux:

```console
$ sudo pacman -S base-devel xorg-fonts-misc qt6-base
```

Fedora (only tested on Fedora 42):

```console
$ sudo dnf install g++ libX11-devel libXtst-devel libXcomposite-devel qt6-qtbase-devel
```

### Compile and run

```console
$ git clone https://github.com/abaudno925-create/Fler.git
$ cd Fler
$ make
$ sudo ./fler
```

`fler` needs root, because it reads the game's memory, and it expects `cs2` to be running.

To look at the menu without the game attached:

```console
$ FLER_GUI_ONLY=1 ./fler
```

## Features

- **ESP** — box, health bar, health text, name, weapon name, skeleton, head dot, cat ears,
  snap lines, team and spotted filters
- **Visuals** — crosshair, sniper crosshair, recoil visualisation, FPS counter
- **Aimbot** — FOV circle, recoil control, auto shoot, rebindable hotkey

## Compatibility

Fler strictly uses X11. The overlay is a transparent, unredirected X window composited over
the game, so a Wayland session works through XWayland as long as the compositor handles
double buffering.

Compositors such as Hyprland have no good XWayland support for double buffering and the
overlay will not render correctly there. A native Wayland backend would fix that.

## Credits

Original work: [Doctor-Coomer/CounterStrike2-Linux-Cheat](https://github.com/Doctor-Coomer/CounterStrike2-Linux-Cheat)
(itself derived from [yoshisaac/CounterStrike2-Linux-Cheat](https://github.com/yoshisaac/CounterStrike2-Linux-Cheat)).
Licensed under the GNU GPL v3 — see `LICENSE`.
