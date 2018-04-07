{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  sptlSrc ? ../.,
  chunkedseq ? ../../chunkedseq,
  buildDocs ? false
}:

stdenv.mkDerivation rec {
  name = "sptl";

  src = sptlSrc;

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