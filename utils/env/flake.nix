{
  description = "orb-firmware flake";
  inputs = {
    # Worlds largest repository of linux software
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.11";
    # Provides eachDefaultSystem and other utility functions
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, utils, }:
    # This helper function is used to more easily abstract
    # over the host platform.
    # See https://github.com/numtide/flake-utils#eachdefaultsystem--system---attrs
    utils.lib.eachDefaultSystem (system:
      let
        p = {
          # The platform that you are running nix on and building from
          native = nixpkgs.legacyPackages.${system};
          # The other platforms we support for cross compilation
          arm-linux = nixpkgs.legacyPackages.aarch64-linux;
          x86-linux = nixpkgs.legacyPackages.x86_64-linux;
          arm-macos = nixpkgs.legacyPackages.aarch64-darwin;
          x86-macos = nixpkgs.legacyPackages.x86_64-darwin;
        };
        pythonShell = (ps: with ps; [
          pyocd
          cmsis-pack-manager
          cffi
        ]);
      in
      # See https://nixos.wiki/wiki/Flakes#Output_schema
      {
        # Everything in here becomes your shell (nix develop)
        devShells.default = p.native.mkShell {
          # Nix makes the following list of dependencies available to the development
          # environment.
          buildInputs = (with p.native; [
            protobuf
            nixpkgs-fmt

            (python3.withPackages pythonShell)
            black

            # This is missing on mac m1 nix, for some reason.
            # see https://stackoverflow.com/a/69732679
            libiconv
          ]);

          # The following sets up environment variables for the shell. These are used
          # by the build.rs build scripts of the rust crates.
          shellHook = ''
            '';
        };
        # Lets you type `nix fmt` to format the flake.
        formatter = p.native.nixpkgs-fmt;
      }
    );
}
