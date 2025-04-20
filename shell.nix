let
  nixpkgs = fetchTarball "https://github.com/NixOS/nixpkgs/tarball/nixos-24.11";
  pkgs = import nixpkgs { config = {}; overlays = []; };
in

pkgs.mkShellNoCC {
  packages = with pkgs; [
    libuuid
    nasm
    qemu
  ];
  shellHook = ''
    unset SOURCE_DATE_EPOCH
  '';
}
