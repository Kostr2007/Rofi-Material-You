# `nix flake check`: build (which runs the unit tests and the theme checks of
# the install phase), render palettes and evaluate the home-manager module.
{ pkgs, self, nixpkgs }:

let
  inherit (pkgs) lib;
  system = pkgs.stdenv.hostPlatform.system;
  package = self.packages.${system}.default;
  mkPalette = self.lib.mkPalette pkgs;
  mkTheme = self.lib.mkTheme pkgs;

  # Parse a generated theme with rofi itself, fail on any warning.
  validate = name: theme: pkgs.runCommandLocal "rofi-material-you-check-${name}" { } ''
    export HOME=$TMPDIR
    ${package}/bin/rofi -theme ${theme} -dump-theme > dump.rasi 2> err.txt || { cat err.txt; exit 1; }
    if [ -s err.txt ]; then cat err.txt; exit 1; fi
    grep -q 'selection-indicator' dump.rasi
    grep -q 'primary' dump.rasi
    ! grep -n '{{' ${theme}
    touch $out
  '';
in
{
  inherit package;
  unwrapped = self.packages.${system}.unwrapped;

  palette-dark = validate "palette-dark" (mkTheme {
    inherit package;
    palette = mkPalette { };
  });

  palette-light-expressive = validate "palette-light" (mkTheme {
    inherit package;
    layout = "grid";
    mode = "light";
    palette = mkPalette { mode = "light"; scheme = "scheme-expressive"; source.color = "#B3261E"; };
  });

  palette-overrides = validate "palette-overrides" (mkTheme {
    inherit package;
    font = "Inter 11";
    width = "640px";
    animations = { motionScheme = "standard"; speed = 1.5; };
    extraConfig = { modes = "drun,run"; };
    extraTheme = ''element-icon selected { shape: "clover-4"; }'';
    palette = mkPalette { overrides = { primary = "#00FF88"; on-primary = "#003300"; }; };
  });

  # The NixOS module installs the package and the fonts.
  nixos-module =
    let
      sys = nixpkgs.lib.nixosSystem {
        modules = [
          self.nixosModules.default
          {
            nixpkgs.hostPlatform = system;
            programs.rofi-material-you.enable = true;
            boot.loader.grub.enable = false;
            fileSystems."/" = { device = "nodev"; fsType = "tmpfs"; };
            system.stateVersion = "26.05";
          }
        ];
      };
    in
    assert builtins.elem package sys.config.environment.systemPackages;
    assert sys.config.fonts.packages != [ ];
    pkgs.runCommandLocal "rofi-material-you-check-nixos" { } "touch $out";

  # The home-manager module evaluates with programs.rofi on and off.
  home-manager-module =
    let
      fileType = lib.types.submodule {
        options = {
          text = lib.mkOption { type = lib.types.nullOr lib.types.lines; default = null; };
          source = lib.mkOption { type = lib.types.nullOr (lib.types.either lib.types.path lib.types.str); default = null; };
        };
      };
      eval = extra: (lib.evalModules {
        modules = [
          self.homeManagerModules.default
          ({ lib, ... }: {
            # The few home-manager options the module touches.
            options = {
              home.packages = lib.mkOption { type = lib.types.listOf lib.types.package; default = [ ]; };
              home.homeDirectory = lib.mkOption { type = lib.types.str; default = "/home/test"; };
              xdg.configFile = lib.mkOption { type = lib.types.attrsOf fileType; default = { }; };
              xdg.dataFile = lib.mkOption { type = lib.types.attrsOf fileType; default = { }; };
              systemd.user = lib.mkOption { type = lib.types.anything; default = { }; };
              programs.rofi = {
                enable = lib.mkOption { type = lib.types.bool; default = false; };
                package = lib.mkOption { type = lib.types.package; default = pkgs.rofi; };
                theme = lib.mkOption { type = lib.types.nullOr lib.types.str; default = null; };
              };
            };
            config._module.args.pkgs = pkgs;
          })
          extra
        ];
      }).config;
      standalone = eval { programs.rofi-material-you.enable = true; };
      withRofi = eval {
        programs.rofi.enable = true;
        programs.rofi-material-you = { enable = true; layout = "grid"; mode = "light"; };
      };
    in
    assert lib.hasInfix "@theme" standalone.xdg.configFile."rofi/config.rasi".text;
    assert withRofi.programs.rofi.theme != null;
    assert withRofi.programs.rofi.package == package;
    validate "home-manager" standalone.programs.rofi-material-you.theme;
}
