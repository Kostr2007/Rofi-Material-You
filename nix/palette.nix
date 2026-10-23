# Generates a Material You palette as rofi theme variables at build time,
# with the same `rofi-myou` the user runs at runtime.
{ lib, runCommandLocal, writeText, rofi-myou-package }:

{ name ? "rofi-material-you-colors"
  # { color = "#6750A4"; } or { image = ./wallpaper.png; } or { json = ./palette.json; }
, source ? { color = "#6750A4"; }
, mode ? "dark"
, scheme ? "scheme-tonal-spot"
, contrast ? 0.0
  # Role overrides, e.g. { primary = "#D0BCFF"; }
, overrides ? { }
  # A complete palette (role -> hex), skips matugen, e.g. the colors of
  # Material-You-Terminal.
, colors ? null
}:

let
  # Roles may be written as on_primary or on-primary.
  norm = lib.replaceStrings [ "-" ] [ "_" ];
  setFlags = lib.concatStringsSep " " (lib.mapAttrsToList
    (role: value: "--set ${lib.escapeShellArg "${norm role}=${value}"}")
    overrides);
  sourceArg =
    if colors != null then
      "${writeText "${name}.json" (builtins.toJSON { colors = lib.mapAttrs' (k: v: lib.nameValuePair (norm k) v) colors; })}"
    else if source ? image then "${source.image}"
    else if source ? json then "${source.json}"
    else source.color;
in
runCommandLocal "${name}.rasi" { } ''
  ${rofi-myou-package}/bin/rofi-myou --out $out apply \
    ${lib.escapeShellArg sourceArg} \
    --mode ${mode} --scheme ${scheme} --contrast ${toString contrast} ${setFlags} --quiet
''
