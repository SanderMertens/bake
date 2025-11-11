{
  description = "Bake - A build system and package manager for C/C++ projects";

  # Key challenges solved in this flake:
  # 1. Bake requires building not just the main binary, but also language drivers
  # 2. Drivers must be built using bake itself (via `bake setup`)
  # 3. Bake needs a writable BAKE_HOME for project metadata, conflicting with
  #    Nix's read-only store
  # 4. Solution: Initialize a writable ~/bake on first run, copied from the Nix store

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

            nativeBuildInputs = [ pkgs.gnumake pkgs.makeWrapper ];

            buildPhase = ''
              runHook preBuild

              # Build main bake binary first using make
              make -C ${buildDir} clean all

              # Use bake itself to build language drivers and utilities
              # This is the proper way to build bake - drivers can't be built with
              # make alone due to complex include path dependencies

              # Set HOME to a temporary directory so bake installs to $HOME/bake
              export HOME=$PWD/fake-home
              mkdir -p $HOME

              # Run bake setup with --local to:
              # - Build bake.util (utility library)
              # - Build bake.lang.c and bake.lang.cpp (language drivers)
              # - Skip /usr/local/bin installation (--local flag)
              ./bake setup --local

              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall

              mkdir -p $out/bin
              mkdir -p $out/share/bake

              # Copy the entire bake environment that was created by bake setup
              # This includes: lib/, bin/, include/, meta/, templates/, etc.
              # Use -L to dereference symlinks - bake setup creates symlinks back to
              # the source directory, which would be dangling in the Nix store
              cp -rL fake-home/bake/* $out/share/bake/

              # Ensure the bake binary is present
              if [ ! -f $out/share/bake/bin/bake ]; then
                mkdir -p $out/share/bake/bin
                cp bake $out/share/bake/bin/bake
              fi

              # Create a wrapper script that handles the read-only Nix store issue
              # Problem: Bake needs BAKE_HOME to be writable (for project metadata),
              #          but Nix store is read-only
              # Solution: On first run, copy the Nix store bake environment to ~/bake
              #           and use that as BAKE_HOME
              cat > $out/bin/bake <<EOF
              #!/bin/sh
              # Initialize ~/bake if it doesn't exist
              if [ ! -d "\$HOME/bake" ]; then
                echo "Initializing bake environment in ~/bake..."
                mkdir -p "\$HOME/bake"
                # Copy the bake environment structure from Nix store
                cp -r "$out/share/bake"/* "\$HOME/bake/"
                # Make files writable (Nix store files are read-only by default)
                chmod -R u+w "\$HOME/bake"
                echo "Bake environment initialized."
              fi
              # Run bake with BAKE_HOME set to user's writable home directory
              export BAKE_HOME="\$HOME/bake"
              exec "$out/share/bake/bin/bake" "\$@"
              EOF
              chmod +x $out/bin/bake

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
