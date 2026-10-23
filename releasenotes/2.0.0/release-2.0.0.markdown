# 2.0.0

In this release we merged back [lbonn](https://github.com/lbonn) his great wayland [port of rofi](https://github.com/lbonn/rofi) into mainline.
Wayland is now an officially supported backend.

## Removal of autotools build system

With the merge of the wayland backend, for ease of maintenance, we dropped the autotools build system. We now only
support the meson build system. Please see the updated
[INSTALL](https://github.com/davatorium/rofi/blob/2.0.0/INSTALL.md)
instructions.

You can build rofi with only X11 or Wayland. It also supports or both backends enabled, where it automatically select the right backend.
[See here](https://github.com/davatorium/rofi/blob/2.0.0/INSTALL.md#options-for-building) for more information about build options.

## Dump info

To help bug reports, we added the command `rofi -info`.
This will print the selected backends. Compilation options, loaded plugins, scripts and more.

An example output:

```
Display backends:
        • xcb
        • wayland: selected

Monitor layout:
              ID: 58
            name: DP-1
           scale: 1
        position: 0,0
            size: 3840,2160
            size: 600mm,340mm  dpi: 163,161


Detected modes:
        • window
        • run
        • +ssh
        • +drun
        • +combi
        • keys
        • filebrowser
        • recursivebrowser

Detected user scripts:

Compile time options:
        • Pango   version 1.56.4
        • window  enabled
        • drun    enabled
        • asan    disabled
        • imdkit  enabled
        • xcb     enabled
        • wayland enabled (1.23.1)

For more information see: man rofi
                 Version: 2.0.0 (next)
              Bugreports: https://github.com/davatorium/rofi/
                 Support: https://github.com/davatorium/rofi/discussions
                          #rofi @ libera.chat
      Configuration file: /home/user/.config/rofi/config.rasi

Parsed files:
        ‣ /home/user/.config/rofi/config.rasi
```

## Known issues

See the [README](https://github.com/davatorium/rofi/blob/2.0.0/README.md) for the full list.

### Redraw on startup

With wayland, rofi only knows after the first surface is drawn, what the dpi and
scale is. Because of this, it might need to redraw/resize elements (icons/text)
after displaying. This can generate a weird 'flicker' on startup. It will first
load a lower resolution version of the icons (that the window manager scales
up), then it gets updated with the real resolution and it then loads full
resolution icons. Because icon loading is asynchronous, they will disappear and
re-appear.

There are possible work-arounds, but those might cause weird artifacts with
window managers that do animations, like hyprland. We are still looking into a
good solution. Help is welcome.

Issue: [#2165](https://github.com/davatorium/rofi/issues/2165)

### Close on click out of window

It is currently not possible, in a reliable way, to detect out of window clicks.
So under wayland this feature currently does not work.

Issue: [#2158](https://github.com/davatorium/rofi/issues/2158)

### No IMDKIT support in wayland

There is currently no imdkit support when using the wayland backend.

Issue: [#131 orig repo](https://github.com/lbonn/rofi/issues/131)

## Changelog

The changelog contains years of wayland patches and merges.
There is little use to list them all here in this releasenote.
Please check the full git log for changes.
