{
  description = "Rocket68: A Motorola 68000 CPU emulator in C";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    zig-overlay = {
      url = "github:mitchellh/zig-overlay";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, zig-overlay }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          zigCompiler = zig-overlay.packages.${system}."0.17.0";
        in
        {
          default = pkgs.mkShell {
            ZIG = "${zigCompiler}/bin/zig";
            packages = with pkgs; [
              # C Toolchain & Build
              gcc
              gnumake

              # Linting, Formatting & Debugging
              clang-tools
              cppcheck

              # Alternative tools you support
              zigCompiler

              # Development Utilities
              pre-commit
              uv

              # Documentation
              doxygen
              (python3.withPackages (ps: with ps; [ mkdocs mkdocs-material mkdocstrings-python ]))
            ] ++ pkgs.lib.optionals pkgs.stdenv.hostPlatform.isLinux [ pkgs.valgrind pkgs.gdb ];
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
