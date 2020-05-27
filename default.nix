{nixpkgs ? import <nixpkgs> {}}:
nixpkgs.stdenv.mkDerivation {
  name = "pez";
  src = ./pez.c;
  buildInputs = [ nixpkgs.libxml2 nixpkgs.curl ];
  dontUnpack = true;
  buildPhase = ''
    gcc `xml2-config --cflags` `xml2-config --libs` -o $name $src -lxml2 -lm -lcurl
  '';
  installPhase = ''
    cp $name $out
  '';
}
