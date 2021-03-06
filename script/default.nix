{ pkgs   ? import <nixpkgs> {},
  stdenv ? pkgs.stdenv,
  sources ? import ./default-sources.nix,
  sptlSrc ? sources.sptlSrc,
  cmdline ? sources.cmdline,
  chunkedseq ? sources.chunkedseq,
  buildDunePackage ? pkgs.ocamlPackages.buildDunePackage,
  benchScript ? import sources.benchScript { sources = sources; },
  gperftools ? pkgs.gperftools,
  gcc ? pkgs.gcc6,
  hwloc ? pkgs.hwloc,
  ocaml ? pkgs.ocaml,
  buildDocs ? false
}:

stdenv.mkDerivation rec {
  name = "sptl";

  src = sptlSrc;

  buildInputs =
    let docs =
      if buildDocs then [
        pkgs.pandoc pkgs.texlive.combined.scheme-small
      ] else [];
    in
    [ cmdline chunkedseq ocaml pkgs.makeWrapper gperftools gcc ] ++ docs;

  configurePhase =
    let settingsScript = pkgs.writeText "settings.sh" ''
      CUSTOM_MALLOC_PREFIX=-ltcmalloc -L${gperftools}/lib
      USE_CILK=1
      CMDLINE_HOME=${cmdline}/include/
      CHUNKEDSEQ_HOME=${chunkedseq}/include/
      SPTL_HOME=${sptlSrc}/include
    '';
    in
    ''
    cp ${settingsScript} autotune/settings.sh
    '';

  buildPhase =
    let doBuildDocs =
      if buildDocs then ''
        make -C doc sptl.pdf sptl.html
      '' else "";
    in
    ''
    ${doBuildDocs}
    make -j -C autotune spawnbench.sptl spawnbench.sptl_elision
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
      mkdir -p $out/autotune/
      cp autotune/spawnbench.sptl autotune/spawnbench.sptl_elision $out/autotune/
      cp ${benchScript}/autotune $out/autotune/autotune.pbench
      mkdir -p $out/bin
      pkgid=`basename $out`
      autotunerespath=/var/tmp/$pkgid
      cat >> $out/bin/autotune <<__EOT__
      #!/usr/bin/env bash
      rm -rf $autotunerespath
      mkdir -p $autotunerespath
      pushd $autotunerespath
      $out/autotune/autotune.pbench find-kappa -skip make
      $out/autotune/autotune.pbench find-alpha -skip make
      popd
      __EOT__
      cat >> $out/include/spautotune.hpp <<__EOT__
      #undef SPTL_PATH_TO_AUTOTUNE_SETTINGS
      #define SPTL_PATH_TO_AUTOTUNE_SETTINGS "$autotunerespath"
      __EOT__
      ln -s $autotunerespath $out/autotune-results
      chmod u+x $out/bin/autotune
      wrapProgram $out/bin/autotune \
       --prefix PATH ":" $out/autotune \
       --prefix PATH ":" ${gcc}/bin \
       --prefix PATH ":" $out/bin \
       --prefix LD_LIBRARY_PATH ":" ${gcc}/lib \
       --prefix LD_LIBRARY_PATH ":" ${gcc}/lib64 \
       --prefix LD_LIBRARY_PATH ":" ${gperftools}/lib \
       --set TCMALLOC_LARGE_ALLOC_REPORT_THRESHOLD 100000000000 \
    '';

  meta = {
    description = "Series Parallel Template Library: a header-only library for writing parallel programs in C++.";
    license = "MIT";
    homepage = http://deepsea.inria.fr/sptl;
  };
}
