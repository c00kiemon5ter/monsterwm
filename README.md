monsterwm
=========

→ tiny and monsterous!
----------------------

**monsterwm** is a minimal, lightweight, tiny but monsterous dynamic tiling window manager.
It will try to stay as small as possible. Currently under 750 lines with the config file included.
It provides a set of different layout modes (see below), and has partial floating mode support.
Each virtual desktop has its own properties, unaffected by other desktops' settings.
For screenshots and ramblings/updates check the [topic on ArchLinux forums][monsterwm].

  [monsterwm]: https://bbs.archlinux.org/viewtopic.php?id=132122


Modes
-----

Monsterwm allows opening the new window as master or
opening the window at the bottom of the stack (attach\_aside)

---

*Common tiling mode:*

    --------------
    |        | W |
    |        |___|
    | Master |   |
    |        |___|
    |        |   |
    --------------

---

*Bottom Stack (bstack) tiling mode:*

    -------------
    |           |
    |  Master   |
    |-----------|
    | W |   |   |
    -------------

---

 *Grid tiling mode:*

    -------------
    |   |   |   |
    |---|---|---|
    |   |   |   |
    |---|---|---|
    |   |   |   |
    -------------

---

 *Monocle mode* (aka fullscreen)

    -------------
    |           |
    | no        |
    | borders!  |
    |           |
    -------------

---

 *floating mode*

    -------------
    |  |        |
    |--'  .---. |
    |     |   | |
    |     |   | |
    ------`---'--

 In floating mode one can freely move and resize windows in the screen space.
 Changing desktops, adding or removing floating windows, does not affect the
 floating status of the windows. Windows will revert to their tiling mode
 position once the user selects a tiling mode.
 Note, that one cannot "select" the floating mode, but it will be enabled if
 one tries to move or resize a window with the mouse. Once one does that, then
 the window is marked as being in floating mode.

---

All accessible with keyboard and mouse shortcuts are defined in the config.h file.

 * The window W at the top of the stack can be resized on a per desktop basis.
 * Changing a tiling mode or window size on one desktop doesn't affect the other desktops.


Panel - Statusbar
-----------------

The user can define an empty space on the bottom or top of the screen, to be
used by a panel. The panel is toggleable, but will be visible if no windows are
on the screen.
Monsterwm does not provide a panel and/or statusbar itself. Instead it adheres
to the [UNIX philosophy][unix] and outputs information about the existent
desktop, the number of windows on each, the mode of each desktop, the current
desktop and urgent hints whenever needed. The user can use whatever tool or
panel suits him best (dzen2, conky, w/e), to process and display that information.

  [unix]: http://en.wikipedia.org/wiki/Unix_philosophy


Installation
------------

You need Xlib, then,
copy `config.def.h` as `config.h`
and edit to suit your needs.
Build and install.

    $ cp config.def.h config.h
    $ $EDITOR config.h
    $ make
    # make clean install


Bugs
----

For any bug or request [fill an issue][bug] on [GitHub][ghp] or report on the [ArchLinux topic][monsterwm]

  [bug]: https://github.com/c00kiemon5ter/monsterwm/issues
  [ghp]: https://github.com/c00kiemon5ter/monsterwm


License
-------

Licensed under MIT/X Consortium License, see [LICENSE][law] file for more copyright and license information.

  [law]: https://raw.github.com/c00kiemon5ter/monsterwm/master/LICENSE

Thanks
------

[the suckless team][skls] for [dwm][],
[moetunes][] for [dminiwm][],
[pyknite][] for [catwm][]

  [skls]: http://suckless.org/
  [dwm]:  http://dwm.suckless.org/
  [moetunes]: https://github.com/moetunes
  [dminiwm]:  https://bbs.archlinux.org/viewtopic.php?id=126463
  [pyknite]: https://github.com/pyknite
  [catwm]:   https://github.com/pyknite/catwm

