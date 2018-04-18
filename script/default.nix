{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  sptlSrc ? ../.,
  cmdline ? ../../cmdline,
  chunkedseq ? ../../chunkedseq,
  pbench ? ../../pbench,
  buildDocs ? false
}:

stdenv.mkDerivation rec {
  name = "sptl";

  src = sptlSrc;

  buildInputs =
    let docs =
      if buildDocs then [
        pkgs.pandoc
        pkgs.texlive.combined.scheme-small
      ] else
        [];
    in
    [ cmdline chunkedseq pbench ] ++ docs;
        
  buildPhase = if buildDocs then ''
    make -C doc sptl.pdf sptl.html
  ''
  else ''
    # nothing to build
  '';  

  installPhase =
    let settingsFile = pkgs.writeText "settings.sh" ''
      CMDLINE_HOME=${cmdline}/include
      CHUNKEDSEQ_HOME=${chunkedseq}/include
    '';
    in
    ''
      mkdir -p $out/include/
      cp include/* $out/include/
      mkdir -p $out/example/
      cp example/* $out/example/
      cp ${settingsFile} $out/example/settings.sh
      mkdir -p $out/doc
      cp doc/sptl.* doc/Makefile $out/doc/
    '';

  meta = {
    description = "Series Parallel Template Library: a header-only library for writing parallel programs in C++.";
    license = "MIT";
    homepage = http://deepsea.inria.fr/sptl;
  };
}
