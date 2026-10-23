{ self }:

{ config, lib, pkgs, options, ... }:

let
  inherit (lib) mkOption mkEnableOption mkIf mkMerge mkDefault types literalExpression;

  cfg = config.programs.rofi-material-you;
  system = pkgs.stdenv.hostPlatform.system;

  defaultPackage = self.packages.${system}.default;

  myt = config.programs.material-you-terminal or { };
  hasTerminal = options ? programs && options.programs ? material-you-terminal;
  terminalEnabled = hasTerminal && (myt.enable or false);

  # Static palette, generated at build time. In dynamic mode `rofi-myou`
  # writes ~/.config/rofi/material-you/colors.rasi at runtime instead, which
  # every theme imports.
  paletteFile =
    if cfg.dynamic.enable then null
    else self.lib.mkPalette pkgs ({
      inherit (cfg) mode scheme contrast;
      overrides = cfg.palette;
    } // (if cfg.source.fromMaterialYouTerminal then {
      colors = myt.colors;
    } else {
      source =
        if cfg.source.image != null then { image = cfg.source.image; }
        else { color = cfg.source.color; };
    }));

  theme = self.lib.mkTheme pkgs {
    package = cfg.package;
    inherit (cfg) layout mode font width extraConfig extraTheme;
    palette = paletteFile;
    animations = cfg.animations;
  };

  wallpaper =
    if cfg.dynamic.wallpaper != null && lib.hasPrefix "~/" cfg.dynamic.wallpaper
    then "${config.home.homeDirectory}/${lib.removePrefix "~/" cfg.dynamic.wallpaper}"
    else cfg.dynamic.wallpaper;

  applyArgs = "--mode ${cfg.mode} --scheme ${cfg.scheme} --contrast ${toString cfg.contrast} "
    + lib.concatStringsSep " " (lib.mapAttrsToList
    (role: value: "--set ${lib.escapeShellArg "${role}=${value}"}")
    cfg.palette);
in
{
  options.programs.rofi-material-you = {
    enable = mkEnableOption ''
      rofi with Material You (Material 3 Expressive) themes and spring
      animations. Works together with `programs.rofi`: when that is enabled
      its package and theme are set, otherwise a config.rasi is written'';

    package = mkOption {
      type = types.package;
      default = defaultPackage;
      defaultText = literalExpression "rofi-material-you.packages.\${system}.default";
      description = ''
        The rofi package. Plugins can be added with
        `pkgs.rofi.override { rofi-unwrapped = rofi-material-you.packages.''${system}.unwrapped; plugins = [ ... ]; }`.
      '';
    };

    layout = mkOption {
      type = types.enum [ "list" "grid" ];
      default = "list";
      description = "`list`: launcher with a search bar and a list, `grid`: app grid like the Android launcher.";
    };

    source = {
      color = mkOption {
        type = types.str;
        default = "#6750A4";
        description = "Seed color of the palette.";
      };
      image = mkOption {
        type = types.nullOr types.path;
        default = null;
        example = literalExpression "./wallpapers/current.jpg";
        description = "Wallpaper to generate the palette from (takes precedence over `color`).";
      };
      fromMaterialYouTerminal = mkOption {
        type = types.bool;
        default = false;
        description = ''
          Use the palette of `programs.material-you-terminal`
          (https://github.com/Kostr2007/Material-You-Terminal), so rofi and
          the terminal always match.
        '';
      };
    };

    mode = mkOption {
      type = types.enum [ "dark" "light" ];
      default = "dark";
      description = "Dark or light scheme.";
    };

    scheme = mkOption {
      type = types.enum [
        "scheme-tonal-spot"
        "scheme-expressive"
        "scheme-vibrant"
        "scheme-fidelity"
        "scheme-content"
        "scheme-rainbow"
        "scheme-fruit-salad"
        "scheme-neutral"
        "scheme-monochrome"
      ];
      default = "scheme-tonal-spot";
      description = "matugen scheme.";
    };

    contrast = mkOption {
      type = types.addCheck types.float (c: c >= -1.0 && c <= 1.0);
      default = 0.0;
      description = "Contrast level, from -1 to 1.";
    };

    palette = mkOption {
      type = types.attrsOf types.str;
      default = { };
      example = { primary = "#D0BCFF"; surface_container = "#1D1B20"; };
      description = "Override single color roles of the generated palette.";
    };

    font = mkOption {
      type = types.nullOr types.str;
      default = null;
      example = "Google Sans 12";
      description = "Pango font description, the theme default is Roboto Flex 12.";
    };

    width = mkOption {
      type = types.nullOr types.str;
      default = null;
      example = "640px";
      description = "Width of the window (any rasi distance).";
    };

    animations = {
      enable = mkOption {
        type = types.bool;
        default = true;
        description = "Spring animations (indicator, shapes, entrance, ripple). `ROFI_NO_ANIMATION=1` disables them per session.";
      };
      motionScheme = mkOption {
        type = types.enum [ "expressive" "standard" ];
        default = "expressive";
        description = "`expressive`: bouncy springs, `standard`: calm, well damped springs.";
      };
      speed = mkOption {
        type = types.addCheck types.float (s: s > 0.0);
        default = 1.0;
        description = "Speed factor, 2.0 is twice as fast.";
      };
      fps = mkOption {
        type = types.ints.between 10 360;
        default = 120;
        description = "Frame rate of the animations.";
      };
    };

    extraConfig = mkOption {
      type = with types; attrsOf (oneOf [ str int bool float ]);
      default = { };
      example = { modes = "drun,run,window"; display-drun = "Apps"; };
      description = "Extra rofi options, written to the `configuration { }` block.";
    };

    extraTheme = mkOption {
      type = types.lines;
      default = "";
      example = ''
        element-icon selected { shape: "clover-4"; }
      '';
      description = "Rasi appended to the theme, see docs/MATERIAL-YOU.md for the properties.";
    };

    theme = mkOption {
      type = types.package;
      readOnly = true;
      description = "The generated theme file.";
    };

    dynamic = {
      enable = mkEnableOption ''
        runtime palettes: `rofi-myou apply <wallpaper>` writes
        ~/.config/rofi/material-you/colors.rasi and the next rofi start uses
        it. When Material-You-Terminal runs in dynamic mode too, rofi follows
        every `myt apply`'';

      wallpaper = mkOption {
        type = types.nullOr types.str;
        default = null;
        example = "~/.cache/current-wallpaper";
        description = "A file (or symlink) to watch, `rofi-myou apply` runs on it whenever it changes.";
      };
    };
  };

  config = mkIf cfg.enable (mkMerge [
    {
      programs.rofi-material-you.theme = theme;
      home.packages = lib.optional (!config.programs.rofi.enable) cfg.package;
      # For people who generate their desktop colors with matugen already.
      xdg.dataFile."rofi/material-you/matugen-rofi.rasi".source =
        "${cfg.package}/share/rofi/material-you/matugen-rofi.rasi";
    }

    (mkIf config.programs.rofi.enable {
      programs.rofi.package = mkDefault cfg.package;
      programs.rofi.theme = mkDefault "${theme}";
    })

    (mkIf (!config.programs.rofi.enable) {
      xdg.configFile."rofi/config.rasi".text = ''
        @theme "${theme}"
      '';
    })

    (mkIf cfg.dynamic.enable (mkMerge [
      (lib.optionalAttrs hasTerminal {
        programs.material-you-terminal.dynamic.hooks =
          lib.mkIf (terminalEnabled && (myt.dynamic.enable or false))
            [ ''${cfg.package}/bin/rofi-myou apply "$MYT_PALETTE" ${applyArgs} --quiet'' ];
      })

      (mkIf (wallpaper != null) {
        systemd.user.paths.rofi-material-you = {
          Unit.Description = "Watch the wallpaper for the rofi Material You palette";
          Path.PathChanged = wallpaper;
          Install.WantedBy = [ "default.target" ];
        };
        systemd.user.services.rofi-material-you = {
          Unit.Description = "Regenerate the rofi Material You palette";
          Service = {
            Type = "oneshot";
            ExecStart = "${cfg.package}/bin/rofi-myou apply ${lib.escapeShellArg wallpaper} ${applyArgs} --quiet";
          };
        };
      })
    ]))
  ]);
}
