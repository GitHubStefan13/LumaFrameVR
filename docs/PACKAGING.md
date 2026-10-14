# Packaging a release

Packaging is CMake. The version comes from the `project(... VERSION ...)` line in the top level
`CMakeLists.txt` and is never hardcoded, so bumping a release is a one line change and every
artifact name follows. `installer` is the target that runs the whole chain, and `package` is an
alias for it.

Packaging produces a release, so the target exists only in a Release build. In any other build
CMake says so at configure time and does not offer the target at all.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target installer    # or: --target package
```

Everything is written under `build/dist`. The target clears that directory first, so it is
repeatable and safe to run as often as you like: `build/dist` is a build directory, not a
published location.

## The chain

1. **The library audit** — `scripts/library-audit.py` records every versioned shared object the
   player links, and whether the payload ships it. It runs against the binary that is about to be
   packaged, so the audit can never describe a different build.
2. **The flat payload** — `cmake/AssemblePayload.cmake`, run with `cmake -P`. It copies the player,
   `launch.sh`, `update.sh`, `update.py`, the branding, the compiled shaders, the language
   catalogues, the eye assets, `LICENSE`, `licenses/` and the library audit into `build/payload`,
   writes `BUILD_INFO.json`, and writes a `SHA256SUMS.txt` covering everything else.
3. **The release archive** — `cmake/PackageRelease.cmake`, also run with `cmake -P`. It verifies the
   payload against the rules below, stages the release under a unique name, adds
   `docs/INSTALLATION.md` as both `INSTALLATION.md` and `README-TEST.md`, bundles the project's own
   source tree as `source/lumaframevr-source.zip`, rebuilds the manifest over the staged tree and
   verifies it again, then archives the tree as `lumaframevr-steam-frame-arm64-<version>.tar.gz`
   with the release directory at the top level, and writes the `.sha256` sidecar beside it. The
   staging tree and the archive are renamed into place only once both are complete, so a failure
   leaves no half-written release. `BUILD_INFO.json` in the release records `payload_origin` (the
   payload's own candidate), `packaged_at_utc`, `installer_format` 3 and the packaging change
   string, and carries any `verification` member the payload had as `payload_verification`.
4. **The installer and the directory bundle** — `scripts/build-installer.py` embeds the archive in
   the self-extracting `lumaframevr-desktop-installer-<version>-<build>.run`, and lays the same
   parts out as separate files plus a double-clickable desktop entry.
5. **The update manifest** — `scripts/update-manifest.py` writes `latest.json` from the archive and
   the installer.

## What the packaging step refuses

The archive is refused, before anything is written, unless the payload holds together. Every one
of these is checked by hashing and reading the tree itself, so a hand-repacked payload is refused
rather than trusted:

- **A Release package.** `BUILD_INFO.json` must say `build_type` is `Release`, and its `candidate`
  must be a safe name: one alphanumeric character followed by up to a hundred more from
  `[A-Za-z0-9._-]`.
- **An AArch64 ELF64 player.** The first twenty bytes of `lumaframevr` must be an ELF64, little
  endian header whose `e_machine` is 183. A release targets the Steam Frame, so anything else is
  refused. The only way past this is the explicit host-architecture opt-in below.
- **A manifest that matches the tree exactly.** Every line of `SHA256SUMS.txt` is
  `<64 hex>  <path>`; every path must be relative and free of `..`, backslashes and duplicates,
  must name a regular file that is not a symlink, and must hash to the value given; the set of
  listed paths must equal the set of files actually present, apart from `SHA256SUMS.txt` itself;
  and the manifest must name `lumaframevr`, `launch.sh`, `BUILD_INFO.json` and `branding/icon.png`.
  A symlink anywhere in the tree is refused.
- **A destination that does not exist.** Neither `<name>` nor `<name>.tar.gz` may already be
  present: a published release is never overwritten.
- **The licence inventory.** `LICENSE` and the whole `licenses/` directory have to be in the
  payload, and assembly fails if the project has no licence to ship. The bundled source tree is
  what satisfies the GPL's corresponding-source requirement for the binary. The archive carries no
  FFmpeg of its own: the player links the frame's, and its licence travels with the frame.

## The archive name

The archive is named for the release and for the target it runs on:

```
lumaframevr-steam-frame-arm64-<version>.tar.gz
```

`<version>` is the project version and `lumaframevr-steam-frame-arm64-` is the fixed prefix. The
same name without the extension is the single top level directory inside the archive, and the
directory the installer installs from. The sidecar is `<archive name>.sha256`, holding one line of
`<sha256>  <archive name>`.

## Packaging on the development host

A native build produces an x86_64 player, which the AArch64 rule refuses by design. To exercise the
whole chain on the development host, opt in explicitly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLUMAFRAMEVR_ALLOW_HOST_ARCHITECTURE=ON
cmake --build build --target installer
```

The player is then accepted although its `e_machine` is not AArch64, and `BUILD_INFO.json` records
`host_architecture`. Such a package is for local testing only and is never a release. A cross
build for the Frame needs no flag.
