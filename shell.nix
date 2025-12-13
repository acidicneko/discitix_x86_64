{ pkgs ? import <nixpkgs> {} }:

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
