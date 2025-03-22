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
            zig
          ];
        };

        packages."bf++" = pkgs.stdenv.mkDerivation {
          name = "bf++";
          src = ./.;
          buildInputs = with pkgs; [
            gcc
            libgccjit
          ];
          buildPhase = ''
            g++ -o bf++ -std=c++23 cpp/bf_gccjit.cpp -lgccjit
          '';
          installPhase = ''
            mkdir -p $out/bin
            cp bf++ $out/bin/
          '';
        };

        packages.bf = pkgs.stdenv.mkDerivation {
          name = "bf";
          src = ./.;
          buildInputs = with pkgs; [
            gcc
            libgccjit
          ];
          buildPhase = ''
            gcc -o bf -std=c23 c/bf_gccjit_c99.c -lgccjit
          '';
          installPhase = ''
            mkdir -p $out/bin
            cp bf $out/bin/
          '';
        };

        apps."bf++" = flake-utils.lib.mkApp {
          drv = self.packages.${system}."bf++";
        };

        apps.bf = flake-utils.lib.mkApp {
          drv = self.packages.${system}.bf;
        };
      }
    );
}

