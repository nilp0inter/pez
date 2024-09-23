{
  description = "A very basic flake";

  outputs = { self, nixpkgs }: {

    packages.x86_64-linux.pez = nixpkgs.legacyPackages.x86_64-linux.callPackage ./default.nix { };

    packages.x86_64-linux.default = self.packages.x86_64-linux.pez;

  };
}
