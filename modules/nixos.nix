{ self }:

{ config, lib, pkgs, options, ... }:

let
  cfg = config.programs.rofi-material-you;
  system = pkgs.stdenv.hostPlatform.system;
in
{
  options.programs.rofi-material-you = {
    enable = lib.mkEnableOption ''
      rofi with Material You themes for all users: the package, `rofi-myou`
      and the default font. Per-user theming is configured with home-manager
      (`home-manager.users.<name>.programs.rofi-material-you`)'';

    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${system}.default;
      defaultText = lib.literalExpression "rofi-material-you.packages.\${system}.default";
      description = "The rofi package installed system-wide.";
    };

    fonts = lib.mkOption {
      type = lib.types.listOf lib.types.package;
      default = lib.filter (p: p != null) [ (pkgs.roboto-flex or null) pkgs.roboto ];
      defaultText = lib.literalExpression "[ pkgs.roboto-flex pkgs.roboto ]";
      description = "Fonts used by the themes.";
    };
  };

  config = lib.mkMerge [
    # Make the home-manager module available to every user when home-manager
    # is used as a NixOS module.
    (lib.optionalAttrs (options ? home-manager) {
      home-manager.sharedModules = [ self.homeManagerModules.default ];
    })

    (lib.mkIf cfg.enable {
      environment.systemPackages = [ cfg.package ];
      fonts.packages = cfg.fonts;
    })
  ];
}
