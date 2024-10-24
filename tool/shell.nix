{ pkgs ? import <nixpkgs> {}, ... }: with pkgs;

stdenv.mkDerivation {
  name = "genode-dev-env";
  buildInputs =
    [ (import ./toolchain.nix { inherit pkgs; })
      gnumake which findutils

      expect libxml2 cdrkit syslinux qemu

      # libc
      subversionClient flex bison

      # virtualbox
      yasm libxslt acpica-tools

      # qt5
      gperf

      # ncurses
      mawk

      # fb_sdl
      pkg-config
      SDL.dev
      alsa-lib.dev
    ];
  dontUnpack = true;

  shellHook =
    ''
      export PROMPT_DIRTRIM=2
      export PS1="\[\033[1;30m\]Genode-dev [\[\033[1;37m\]\w\[\033[1;30m\]] $\[\033[0m\] "
      export PS2="\[\033[1;30m\]>\[\033[0m\] "
    '';
}
