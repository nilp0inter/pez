{ stdenv, libxml2, curl }:
stdenv.mkDerivation rec {
  name = "pez";
  src = ./pez.c;
  buildInputs = [ libxml2 curl ];
  dontUnpack = true;
  buildPhase = ''
    gcc `xml2-config --cflags` `xml2-config --libs` -o $name $src -lxml2 -lm -lcurl
  '';
  installPhase = ''
    install -D -m755 ${name} $out/bin/${name}
  '';
}
