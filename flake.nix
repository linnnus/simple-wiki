{
  description = "Flake utils demo";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/23.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        simple-wiki = pkgs.stdenv.mkDerivation {
          pname = "simple-wiki";
          version = "0.0.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [ pkg-config ];
          buildInputs = with pkgs; [ libgit2 ];

          installPhase = ''
            mkdir -p $out
            make PREFIX=$out install
          '';
        };

        creole-test = pkgs.stdenv.mkDerivation rec {
          pname = "creole-test";
          version = simple-wiki.version;

          src = ./.;

          nativeBuildInputs = with pkgs; [ pkg-config ];
          buildInputs = with pkgs; [ libgit2 ];

          installPhase = ''
            mkdir -p $out/bin
            make build/creole-test
            mv build/creole-test $out/bin
          '';
        };

        devShell = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [ pkg-config ];
          buildInputs = with pkgs; [ libgit2 ];
        };
      in
      {
        packages = {
          inherit simple-wiki creole-test;
          default = simple-wiki;
        };

        devShells.default = devShell;
      }
    );
}
