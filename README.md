# sowm (*~~Simple~~ Shitty Opinionated Window Manager*)

- Floating only.
- Fullscreen toggle (fixed).
- Window centering (fixed).
- Mix of mouse and keyboard workflow.
- Focus with the cursor.
- Infinite canvas
- Minimap for the infinite canvas
- Rounded corners (added)
- change display with key-bind

- Alt-Tab window focusing.
- All windows die on exit.
- Partial EWMH support
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
| `MOD1` + `b` | Toggle the minimap |

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

# Experimental Options 

In the `sowm.h`, you can turn on an experimental option called TITLEBAR
This option is specifically experimental, so if it doesn't work properly with some applications, please report, and the patches are welcome

## Dependencies

- `xlib` (*usually `libX11`*).
- `Xinerama `(By Xlib).

## Thanks

- [sowm](https://github.com/dylanaraps/sowm) (where all of this started)
- [sowm-extended](https://github.com/kantiankant/sowm-extended) (For your help with the code)
- [2bwm](https://github.com/venam/2bwm)
- [SmallWM](https://github.com/adamnew123456/SmallWM)
- [berry](https://github.com/JLErvin/berry)
- [catwm](https://github.com/pyknite/catwm)
- [dminiwm](https://github.com/moetunes/dminiwm)
- [dwm](https://dwm.suckless.org)
- [monsterwm](https://github.com/c00kiemon5ter/monsterwm)
- [openbox](https://github.com/danakj/openbox)
- [possum-wm](https://github.com/duckinator/possum-wm)
- [swm](https://github.com/dcat/swm)
- [tinywm](http://incise.org/tinywm.html)
