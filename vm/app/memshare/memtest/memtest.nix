{ pkgs ? import <nixpkgs> {} }: pkgs.pkgsStatic.callPackage (

{ lib, stdenv, gcc }:

let
  inherit (lib) cleanSource cleanSourceWith hasSuffix;
in

stdenv.mkDerivation {
  name = "memtest";

  BUILD_TARGET = "\"Ala ma 41kota!\"";
  src = cleanSourceWith {
    filter = name: _type: !(hasSuffix ".nix" name);
    src = cleanSource ./.;
  };

  doCheck = true;

}
) { }
