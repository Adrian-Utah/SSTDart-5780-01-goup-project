{
  description = "SSTDart ECE 5780 group project";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem
      (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
          {
            devShells.default = pkgs.mkShell {
              buildInputs = with pkgs; [
                binutils
                cmake
                gcc-arm-embedded
                gnumake
                openocd
                stlink
              ];
            };
          }
      );
}
