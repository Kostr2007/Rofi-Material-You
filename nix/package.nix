# rofi with the Material You (Material 3 Expressive) changes, built from this
# repository. Same inputs as nixpkgs' rofi-unwrapped, plus the `rofi-myou`
# palette generator.
{ lib
, stdenv
, buildPackages
, bison
, cairo
, check
, flex
, glib
, librsvg
, libstartup_notification
, libxcb
, libxkbcommon
, makeWrapper
, matugen
, meson
, ninja
, pandoc
, pango
, pkg-config
, python3
, wayland
, wayland-protocols
, wayland-scanner
, which
, xcb-imdkit
, libxcb-util
, libxcb-cursor
, libxcb-keysyms
, libxcb-wm
, xcbutilxrm
, waylandSupport ? stdenv.hostPlatform.isLinux
, x11Support ? true
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "rofi-material-you-unwrapped";
  version = "2.0.0-unstable-2026-10-01-my1.0.0";

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.difference ../. (lib.fileset.unions [
      ../nix
      ../modules
      ../flake.nix
      (lib.fileset.maybeMissing ../flake.lock)
      (lib.fileset.maybeMissing ../docs/screenshots)
      (lib.fileset.maybeMissing ../.github)
    ]);
  };

  preConfigure = ''
    patchShebangs script
  '';

  depsBuildBuild = [
    buildPackages.stdenv.cc
    glib
    pkg-config
  ];

  nativeBuildInputs = [
    bison
    flex
    makeWrapper
    meson
    ninja
    pandoc
    pkg-config
  ] ++ lib.optionals waylandSupport [
    wayland-protocols
    wayland-scanner
  ];

  buildInputs = [
    cairo
    check
    librsvg
    libstartup_notification
    libxkbcommon
    pango
    python3
    which
  ] ++ lib.optionals waylandSupport [
    wayland
    wayland-protocols
  ] ++ lib.optionals x11Support [
    libxcb
    xcb-imdkit
    libxcb-util
    libxcb-cursor
    libxcb-keysyms
    libxcb-wm
    xcbutilxrm
  ];

  mesonFlags = [
    (lib.mesonBool "imdkit" x11Support)
    (lib.mesonEnable "wayland" waylandSupport)
    (lib.mesonEnable "xcb" x11Support)
    (lib.mesonEnable "check" true)
  ];

  doCheck = true;

  postInstall = ''
    wrapProgram $out/bin/rofi-myou --prefix PATH : ${lib.makeBinPath [ matugen ]}
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    runHook preInstallCheck
    $out/bin/rofi -version
    $out/bin/rofi-myou --version
    for t in material-you material-you-light material-you-grid material-you-grid-light; do
      HOME=$TMPDIR $out/bin/rofi -theme $t -dump-theme > $TMPDIR/$t.rasi
      grep -q selection-indicator $TMPDIR/$t.rasi
    done
    runHook postInstallCheck
  '';

  passthru.themes = [
    "material-you"
    "material-you-light"
    "material-you-grid"
    "material-you-grid-light"
  ];

  meta = {
    description = "rofi with Material You (Material 3 Expressive) themes and spring animations";
    homepage = "https://github.com/Kostr2007/Rofi-Material-You";
    license = lib.licenses.mit;
    platforms = lib.platforms.linux;
    mainProgram = "rofi";
  };
})
