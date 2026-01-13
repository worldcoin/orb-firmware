{
  description = "orb-firmware flake - extends orb-software environment";
  inputs = {
    # Import orb-software flake as base environment
    # Override with local path via: --override-input orb-software path:/path/to/orb-software
    orb-software.url = "github:worldcoin/orb-software";

    # Share nixpkgs with orb-software to avoid duplicates
    nixpkgs.follows = "orb-software/nixpkgs";
    nixpkgs-unstable.follows = "orb-software/nixpkgs-unstable";

    # Provides eachDefaultSystem and other utility functions
    flake-utils.follows = "orb-software/flake-utils";

    # Zephyr SDK support (just for SDK and host tools)
    zephyr-nix.url = "github:nix-community/zephyr-nix";
    zephyr-nix.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, orb-software, nixpkgs, nixpkgs-unstable, flake-utils, zephyr-nix, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };
        pkgs-unstable = import nixpkgs-unstable {
          inherit system;
          config.allowUnfree = true;
        };

        # Get the orb-software devShell for this system
        orbSoftwareShell = orb-software.devShells.${system}.default;

        zephyr = zephyr-nix.packages.${system};

        # Python environment with firmware-specific packages
        # Using unstable nixpkgs for better Python package availability
        # For packages not included (canopen), use: pip install --user canopen
        pythonEnv = pkgs-unstable.python312.withPackages (ps: with ps; [
          # Core packages
          pip # for installing additional packages not in nixpkgs
          pyserial
          pyyaml
          colorama
          click
          jinja2
          packaging
          requests
          tqdm

          # Build/testing
          pytest
          coverage

          # Embedded development
          intelhex
          pyelftools
          pyusb
          pyftdi

          # Zephyr/embedded tools
          west # Zephyr meta tool
          pyocd # ARM Cortex-M debugger
          cmsis-pack-manager # CMSIS pack management

          # CAN bus
          python-can
          # canopen - install via pip if needed (nixpkgs version has test failures)

          # CBOR serialization
          cbor2

          # Git/versioning
          gitpython

          # Data analysis
          numpy
          matplotlib

          # Code quality
          pylint
          mypy

          # Documentation
          tabulate

          # Protocol buffers
          grpcio
          grpcio-tools
          protobuf

          # Misc utilities
          psutil
          pycparser
          cffi
          cryptography
          lxml
          pillow
        ]);

      in
      {
        devShells.default = pkgs.mkShell {
          # Inherit everything from orb-software shell
          inputsFrom = [ orbSoftwareShell ];

          # Add firmware-specific build dependencies
          buildInputs = (with pkgs; [
            # Build system
            cmake
            ninja
            gnumake
            ccache
            gperf
            dtc

            # C/C++ tooling
            clang-tools # includes clang-format, clang-tidy
            llvmPackages.libclang

            # Python environment with all packages
            pythonEnv

            # Protocol buffers
            protobuf

            # Debugging and flashing
            openocd

            # USB support
            libusb1

            # Security/crypto
            gnupg
            openssl

            # Compression
            zstd
            xxHash

            # Misc
            graphviz # for dependency graphs
          ]) ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
            # Linux-specific packages for USB/device access
            pkgs.udev
          ] ++ pkgs.lib.optionals pkgs.stdenv.isDarwin [
            # macOS-specific packages
            pkgs.libiconv
          ] ++ [
            # Zephyr SDK with ARM toolchain (add more targets as needed)
            (zephyr.sdk-0_16.override {
              targets = [
                "arm-zephyr-eabi"
                # Add more targets if needed:
                # "riscv64-zephyr-elf"
                # "x86_64-zephyr-elf"
              ];
            })
          ];

          shellHook = ''
            # Inherit orb-software shell hook
            ${orbSoftwareShell.shellHook or ""}

            # Firmware-specific environment variables
            export ZEPHYR_TOOLCHAIN_VARIANT=zephyr

            # Ensure ccache is used
            export USE_CCACHE=1

            # Fix Python site-packages path for the firmware Python environment
            # This ensures protobuf and other packages are found
            export PYTHONPATH="${pythonEnv}/${pythonEnv.sitePackages}''${PYTHONPATH:+:$PYTHONPATH}"

            # Add local scripts to PATH if they exist
            if [ -d "$PWD/utils" ]; then
              export PATH="$PWD/utils:$PATH"
            fi

            echo ""
            echo "🔧 orb-firmware development environment loaded"
            echo "   Extends orb-software with firmware-specific tools"
            echo "   Zephyr SDK: 0.16.x (compatible with Zephyr 3.5.x - 4.1.x)"
            echo ""
          '';
        };

        # Lets you type `nix fmt` to format the flake.
        formatter = pkgs.nixpkgs-fmt;
      }
    );
}
