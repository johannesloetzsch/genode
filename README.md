# shell.nix + flake.nix for Genode development

A trivial shell.nix, providing the dependencies from [./doc/getting_started.txt](./doc/getting_started.txt#L24).  
It might be helpfull, if you want to follow the workflows from the [official documentation](https://genode.org/documentation/).

<img src="https://upload.wikimedia.org/wikipedia/commons/b/bc/Genode_logo_text.png" height="60px" align="center"/> +
<img src="https://raw.githubusercontent.com/NixOS/nixos-artwork/refs/heads/master/logo/nix-snowflake-rainbow.svg" height="60px" align="center"/>

## Usage

```sh
nix develop github:johannesloetzsch/genode/nix
```

Or without flakes:
```sh
nix-shell tool/shell.nix
```

## Alternative: [Genode Nixpkgs](https://github.com/zgzollers/nixpkgs-genode/)

If you aim for simply building Genode packages based on NixFlakes, you may want use the Genode `flake template` from [github:zgzollers/nixpkgs-genode](https://github.com/zgzollers/nixpkgs-genode).

The "hello" application and runscript from the [Genode Foundations book](https://genode.org/documentation/genode-foundations/23.05/index.html) can be build and started by:

```sh
nix flake new -t github:zgzollers/nixpkgs-genode#genode ./genode-hello; cd genode-hello
nix build
qemu-system-x86_64 -cdrom result -m 64 -nographic  ## press Ctrl+A X to quit qemu
```



