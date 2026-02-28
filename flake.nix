{
  description = "Rocket68: A Motorola 68000 CPU emulator in C";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              # C Toolchain & Build
              gcc
              gnumake

              # Linting, Formatting & Debugging
              clang-tools
              cppcheck
              valgrind
              gdb

              # Alternative tools you support
              zig

              # Development Utilities
              pre-commit

              # Documentation
              doxygen
              (python3.withPackages (ps: with ps; [ mkdocs uv ]))
            ];
          };
        }
      );

      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.stdenv.mkDerivation {
            name = "rocket68";
            src = ./.;

            buildPhase = ''
              make all
            '';

            installPhase = ''
              mkdir -p $out/lib $out/include
              if [ -f lib/librocket68.a ]; then
                cp lib/librocket68.a $out/lib/
              fi
              if [ -f lib/librocket68.so ]; then
                cp lib/librocket68.so $out/lib/
              fi
              if [ -d include ]; then
                cp include/* $out/include/
              fi
            '';
          };
        }
      );
    };
}
