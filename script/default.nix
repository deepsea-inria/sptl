{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  fetchurl,
  chunkedseq,
  buildDocs ? false
}:

stdenv.mkDerivation rec {
  name = "sptl-${version}";
  version = "v0.1-alpha";

  src = fetchurl {
    url = "https://github.com/deepsea-inria/sptl/archive/${version}.tar.gz";
    sha256 = "1bzzgadfcm4qfw6jl45rcfqkh39zmgr2fasp830fcfvzyyfc9phx";
  };

  buildInputs =
    let docs =
      if buildDocs then [
        pkgs.pandoc
        pkgs.texlive.combined.scheme-full
      ] else
        [];
    in
    [ chunkedseq ] ++ docs;
        
  buildPhase = if buildDocs then ''
    make -C doc sptl.pdf sptl.html
  ''
  else ''
    # nothing to build
  '';

  installPhase = ''
    mkdir -p $out/include/
    cp include/* $out/include/
    mkdir -p $out/example/
    cp example/* $out/example/
    mkdir -p $out/doc
    cp doc/sptl.* doc/Makefile $out/doc/
  '';

  meta = {
    description = "Series Parallel Template Library: a header-only library for writing parallel programs in C++.";
    license = "MIT";
    homepage = http://deepsea.inria.fr/sptl;
  };
}