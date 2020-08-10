let pkgs = import <nixpkgs> {}; in

let
  projectSrc = ../.;
  pbenchSrc = pkgs.fetchFromGitHub {
    owner  = "mikerainey";
      repo   = "pbench";
      rev    = "1c90259b594b6612bc6b9973564e89c297ad17b3";
      sha256 = "1440zavl3v74hcyg49h026vghhj1rv5lhfsb5rgfzmndfynzz7z0";
  };
  cmdlineSrc = pkgs.fetchFromGitHub {
    owner  = "deepsea-inria";
    repo   = "cmdline";
    rev    = "67b01773169de11bf04253347dd1087e5863a874";
    sha256 = "1bzmxdmnp7kn6arv3cy0h4a6xk03y7wdg0mdkayqv5qsisppazmg";
  };
  chunkedseqSrc = pkgs.fetchFromGitHub {
    owner  = "deepsea-inria";
    repo   = "chunkedseq";
    rev    = "d2925cf385fb43aff7eeb9c08cce88d321e5e02e";
    sha256 = "09qyv48vb2ispl3zrxmvbziwf6vzjh3la7vl115qgmkq67cxv78b";
  };
in


{
  cmdline = import "${cmdlineSrc}/script/default.nix" {};
  
  chunkedseq = import "${chunkedseqSrc}/script/default.nix" {};
  
  pbenchSrc = pbenchSrc;

  pbenchOcamlSrcs = import "${pbenchSrc}/nix/local-sources.nix";

  benchOcamlSrc = "${projectSrc}/autotune/";

  sptlSrc = projectSrc;

  benchScript = "${projectSrc}/script/bench-script.nix";
  
}
