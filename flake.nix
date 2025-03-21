{
  description = "C++ development environment with libgccjit";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            gcc
            libgccjit
            # tinycc
          ];
        };

        packages.default = pkgs.stdenv.mkDerivation {
          name = "my-cpp-app";
          src = ./.; # ソースコードのディレクトリ
          buildInputs = with pkgs; [
            gcc
            libgccjit
            # tinycc
          ];
          buildPhase = ''
            g++ -o my-app main.cpp -lgccjit
          '';
          installPhase = ''
            mkdir -p $out/bin
            cp my-app $out/bin/
          '';
        };

        apps.default = flake-utils.lib.mkApp {
          drv = self.packages.${system}.default;
        };
      }
    );
}

