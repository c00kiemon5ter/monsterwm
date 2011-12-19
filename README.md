monsterwm
=========

### tiny and monsterous!

This is a branch off of moetunes [dminiwm][], which in turn was based on [catwm][] by [pyknite][].
Credits and licensing included in `monsterwm.c` and/or `config.def.h`.
For screenshots and ramblings/updates check the [topic on ArchLinux forums][monsterwm].

  [pyknite]: https://github.com/pyknite
  [catwm]: https://bbs.archlinux.org/viewtopic.php?id=100215
  [dminiwm]: https://bbs.archlinux.org/viewtopic.php?id=126463
  [monsterwm]: https://bbs.archlinux.org/viewtopic.php?pid=1029955


Summary
-------

**monsterwm** is a very minimal, lightweight, monsterous, tiny, dynamic tiling window manager.
It will try to stay as small as possible. Currently under 700 lines with the config file included.
It's like dwm with gridlayout, bstack, pertag, dwmreturn patches builtin, but without floating mode.

Modes
-----

It allows the "normal" method of tiling window managers (with the new window as the master)
and with the new window opened at the bottom of the stack (like `dwm`'s attach\_aside)

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
    |           |
    |           |
    |           |
    -------------

---

All accessible with keyboard shortcuts defined in the config.h file.

 * The window W at the top of the stack can be resized on a per desktop basis.
 * Changing a tiling mode or window size on one desktop doesn't affect the other desktops.

One can also define an empty space on the bottom or top of the screen, to be used by a panel.
The panel is toggleable, but will be visible if no windows are on the screen.

Installation
------------

You need Xlib, then,
copy the `config.def.h` file as `config.h`
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

