# 1.7.9

## Custom commands on events

Giomatfois62 added a nice feature that allows you to execute a custom command
on different events. For example play a sound when accepting an entry.

See the
[rofi-actions(5)](https://github.com/davatorium/rofi/blob/1.7.9/doc/rofi-actions.5.markdown)
manpage for more information.

## NVidia workaround workaround

Because of oddness in nvidia drivers, we had an issue the whole screen turned
black if we used the 'over' operator in cairo. Working around this caused some
drawing issues with anti-aliasing. There now exists two flags to to work around
this workaround again. Either disable the workaround, or disable anti-aliasing.

You can set these options per widget:

```css
widget {
  border-disable-nvidia-workaround: true;
  border-aa: false;
}
```

It's recommended to set it globally.

```css
* {
  border-disable-nvidia-workaround: true;
  border-aa: false;
}
```

## IMDKit runtime disable option

Because IMDKit can break some keybindings, this can now be disabled at runtime.
For example if you try to bind only the 'Super' key, this can fix this.

For more information, see issue: #2124

## Smartcase support

Thanks to Phanium, rofi now supports Vim style 'smartcase'. Can be enabled
using `-case-smart`.

Fore more information, see issue: #2060

## Changelog

* Add -imdkit config to toggle imdkit at runtime (#2124) (thx Amos Bird)
* [Widget] Add 2 workaround for error in corner radius drawing
* [rofi-sensible-terminal] Add ghostty (#2110)
* [scrollbar] Add theming for rounded corners (#2108) (thx J0hannes101)
* [DOC] Fix indenting of sub-bullets as mkdocs interpets them differently (#2112)
* Fix typo in fullscreen-preview.rasi (#2113) (thx elig0n)
* Add theming to hide text cursor if no text is put in (#2106) (thx J0hannes101)
* [DRUN] Add option to exclude categories from drun #2102 (#2104) (thx J0hannes101)
* [XCB] Don't keep casting from int to double to int
* [Helper] Add a rofi_fallthrough macro to tell compiler fallthrough is intentional
* [View] xcb_clear_area  fixed wrong argument order.
* [DRun] Better handling of corrupted cache file.
* Fix broken Pango link (#2096) (thx Emil)
* [NKUtils] Drop support for binding Meta key. (#2095)
* [View] Allow cycling through matching methods (#2091)
* Fix wrong pointer `-replace` and small cleanup.
* [Textbox] Small signedness fixes for password mask code.
* [dmenu] Modified textbox password masking (#2067) (thx Zebra2711)
* add smartcase support like vim (#2060) (thx Phanium)
* [Mode] Fix wrong documentation header.
* Execute custom user commands or scripts on a variety of rofi events (#2053) (thx giomatfois62)
* Add option to enable the `kb-delete-entry` in script mode (#2087) (thx Tiou Lims)
* [Window] Allow active window to be sorted on top. (#2048)
* [Lexer] Allow for optional imports. (#2078)
