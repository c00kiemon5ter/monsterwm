monsterwm
=========

### ~ it's minimal and dynamic

[moetunes][] started this based on [catwm][] 31/12/10 ([ArchLinux forums][catf]) creating [dminiwm][]
Credits and licensing included in `monsterwm.c` and/or `config.def.h`.
For screenshots and ramblings/updates check [the topic on ArchLinux forums][dminif]

  [moetunes]: https://github.com/moetunes
  [catwm]: https://github.com/pyknite/catwm
  [catf]: https://bbs.archlinux.org/viewtopic.php?id=100215&p=1
  [dminiwm]: https://github.com/moetunes/dminiwm
  [dminif]: https://bbs.archlinux.org/viewtopic.php?id=126463


Summary
-------

**monsterwm** is a very minimal, lightweight, monsterous, dynamic tiling window manager.
It will try to stay as small as possible. Currently under 700 lines with the config file included.

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

For any bug or request [fill an issue][bug] on [GitHub][ghp]

  [bug]: https://github.com/c00kiemon5ter/monsterwm/issues
  [ghp]: https://github.com/c00kiemon5ter/monsterwm

