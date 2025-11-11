{
  description = "Bake - A build system and package manager for C/C++ projects";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        # Map Nix system to bake build directory
        buildDirMap = {
          "x86_64-linux" = "build-Linux";
          "aarch64-linux" = "build-Linux";
          "x86_64-darwin" = "build-Darwin";
          "aarch64-darwin" = "build-Darwin";
        };

        buildDir = buildDirMap.${system} or (throw "Unsupported system: ${system}");

      in {
        packages = {
          default = pkgs.stdenv.mkDerivation rec {
            pname = "bake";
            version = "2.5.0";

            src = ./.;

            nativeBuildInputs = [ pkgs.gnumake ];

            buildPhase = ''
              runHook preBuild
              make -C ${buildDir} clean all
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall

              mkdir -p $out/bin
              mkdir -p $out/share/bake

              # Install the bake binary
              install -Dm755 bake $out/bin/bake

              # Install necessary runtime files
              cp -r drivers $out/share/bake/
              cp -r templates $out/share/bake/
              cp -r examples $out/share/bake/

              # Copy include and src for driver builds
              cp -r include $out/share/bake/
              cp -r src $out/share/bake/
              cp -r util $out/share/bake/

              runHook postInstall
            '';

            meta = with pkgs.lib; {
              description = "A build system and package manager for C/C++ projects";
              longDescription = ''
                Bake is a build tool that makes building C/C++ code effortless.
                It provides minimal, platform independent project configuration,
                automatic dependency resolution, and builtin support for multiple
                compilers including gcc, clang, msvc, and emscripten.
              '';
              homepage = "https://github.com/SanderMertens/bake";
              license = licenses.gpl3Only;
              maintainers = [ ];
              platforms = platforms.unix;
              mainProgram = "bake";
            };
          };
        };

        apps.default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/bake";
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            gnumake
            gcc
            gdb
          ];

          shellHook = ''
            echo "Bake development environment"
            echo "Run 'make -C build-\$(uname) clean all' to build"
          '';
        };
      }
    );
}
