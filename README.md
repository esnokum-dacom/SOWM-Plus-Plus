# sowm (*~~Simple~~ Shitty Opinionated Window Manager*)

- Floating only.
- Fullscreen toggle (fixed).
- Window centering (fixed).
- Mix of mouse and keyboard workflow.
- Focus with the cursor.
- Rounded corners (added)
- change display with key-bind

- Alt-Tab window focusing.
- All windows die on exit.
- No window borders.
- No EWMH.

## Default Keybindings

**Window Management**

You can change the keybinding in the `config.de.h`

| combo                      | action                 |
| -------------------------- | -----------------------|
| `Mouse`                    | focus under cursor     |
| `MOD1` + `Left Mouse`      | move window            |
| `MOD1` + `Right Mouse`     | resize window          |
| `MOD1` + `f`               | maximize toggle        |
| `MOD1` + `c`               | center window          |
| `MOD1` + `Shift` + `c`     | kill window            |
| `MOD1` + `1-6`             | desktop swap           |
| `MOD1` + `Shift` +`1-6`    | send window to desktop |
| `MOD1` + `TAB` (*alt-tab*) | focus cycle            |
| `MOD1` + `Shift` + `Left`  | Move the canvas to the Left |
| `MOD1` + `Shift` + `Right` | Move the canvas to the Right |
| `MOD1` + `Shift` + `Up`    | Move the canvas to the Up |
| `MOD1` + `Shift` + `Down`  | Move the canvas to the Down |
| `Mouse wheel` (Press)      | Move the canvas with the mouse position |

**Programs**

| combo                    | action           | program        |
| ------------------------ | ---------------- | -------------- |
| `MOD4` + `Return`        | terminal         | `st`           |
| `MOD4` + `p`             | dmenu            | `dmenu_run`    |
| `MOD4` + `Shift` + `s`   | scrot            | `scr`          |
| `XF86_AudioLowerVolume`  | volume down      | `amixer`       |
| `XF86_AudioRaiseVolume`  | volume up        | `amixer`       |
| `XF86_AudioMute`         | volume toggle    | `amixer`       |
| `XF86_MonBrightnessUp`   | brightness up    | `bri`          |
| `XF86_MonBrightnessDown` | brightness down  | `bri`          |

## Dependencies

- `xlib` (*usually `libX11`*).
- `Xinerama `(By Xlib).

# TODO
- add Zoom functionallity
- Add title bar in upside the windows
