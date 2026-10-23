{
  description = "rofi with Material You (Material 3 Expressive) themes: springy animations, M3 shapes and palettes from your wallpaper";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      inherit (nixpkgs) lib;
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      # Package names changed in nixpkgs (xorg.xcbutil -> libxcb-util, ...),
      # support both.
      unwrappedFor = pkgs: pkgs.callPackage ./nix/package.nix {
        libxcb-util = pkgs.libxcb-util or pkgs.xorg.xcbutil;
        libxcb-cursor = pkgs.libxcb-cursor or pkgs.xcb-util-cursor;
        libxcb-keysyms = pkgs.libxcb-keysyms or pkgs.xorg.xcbutilkeysyms;
        libxcb-wm = pkgs.libxcb-wm or pkgs.xorg.xcbutilwm;
        libxcb = pkgs.libxcb or pkgs.xorg.libxcb;
        xcbutilxrm = pkgs.xcbutilxrm or pkgs.xorg.xcbutilxrm;
      };
      # nixpkgs' rofi wrapper (icon themes, SVG loader, plugins) around our build.
      wrappedFor = pkgs: pkgs.rofi.override { rofi-unwrapped = unwrappedFor pkgs; };
    in
    {
      overlays.default = final: _prev: {
        rofi-material-you-unwrapped = unwrappedFor final;
        rofi-material-you = wrappedFor final;
      };

      packages = forAllSystems (pkgs: {
        default = wrappedFor pkgs;
        rofi-material-you = wrappedFor pkgs;
        unwrapped = unwrappedFor pkgs;
        # `nix build .#palette-preview` renders the default dark palette.
        palette-preview = self.lib.mkPalette pkgs { };
      });

      lib = {
        # mkPalette pkgs { source = { image = ./wall.png; }; mode = "dark"; ... }
        mkPalette = pkgs: args: pkgs.callPackage ./nix/palette.nix
          { rofi-myou-package = unwrappedFor pkgs; }
          args;
        # mkTheme pkgs { package = ...; layout = "grid"; palette = ...; ... }
        mkTheme = pkgs: args: pkgs.callPackage ./nix/theme.nix { } args;
      };

      homeManagerModules = {
        default = self.homeManagerModules.rofi-material-you;
        rofi-material-you = import ./modules/home-manager.nix { inherit self; };
      };

      nixosModules = {
        default = self.nixosModules.rofi-material-you;
        rofi-material-you = import ./modules/nixos.nix { inherit self; };
      };

      checks = forAllSystems (pkgs: import ./nix/checks.nix { inherit pkgs self nixpkgs; });

      formatter = forAllSystems (pkgs: pkgs.nixpkgs-fmt);

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ (unwrappedFor pkgs) ];
          packages = with pkgs; [ matugen python3 xvfb-run xdotool imagemagick nixpkgs-fmt ];
        };
      });
    };
}
