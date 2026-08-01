{
  description = "Development shell with x86_64 freestanding cross-compiler";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            nativeBuildInputs =
              with pkgs; [
                cmake
                ninja
                gcc
                pkgsCross.x86_64-embedded.buildPackages.gcc
                grub2
                qemu
                xorriso
                gtest
                just
                ccache
              ] ++ lib.optionals stdenv.hostPlatform.isLinux [
                mold
              ];
          };
        }
      );
    };
}
