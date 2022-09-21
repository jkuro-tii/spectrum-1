{ pkgs ? import <nixpkgs> {} }: pkgs.pkgsStatic.callPackage (

{ lib, stdenv, gcc }:

let
  inherit (lib) cleanSource cleanSourceWith hasSuffix;
in

stdenv.mkDerivation {
  name = "memtest";

  BUILD_TARGET = "\"This message may be passed to a C program\"";
  src = cleanSourceWith {
    filter = name: _type: !(hasSuffix ".nix" name);
    src = cleanSource ./.;
  };

  doCheck = true;

}
) { }
