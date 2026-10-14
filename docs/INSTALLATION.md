# Installing LumaFrameVR on Steam Frame

## Requirements

Run as your normal desktop user, without sudo. Normal installation uses POSIX sh
and standard Linux utilities (tar, sha256sum, coreutils); it needs neither root nor
Python. Running the player requires ARM64 Linux and an existing SteamVR/OpenXR
runtime. Installation does not install system packages or configure that runtime.
Python 3, using only its standard library, is needed **only** for the optional
Steam non-Steam-game shortcut.

**SteamVR reads its shortcut list when it starts.** If you install with `--steam` while Steam or
SteamVR is running, restart it before LumaFrameVR appears under Non-Steam Games: Steam keeps that
list in memory and writes its own copy back when it exits, which undoes a shortcut added while it
was running. The installer warns when it notices Steam running, and the console output says so
either way.

## Download and run

Download these GitHub Release assets to the same folder:

- `lumaframevr-desktop-installer.run`
- `lumaframevr-desktop-installer.run.sha256`

From that folder, verify the checksum before making the installer executable:

```sh
sha256sum -c lumaframevr-desktop-installer.run.sha256
chmod +x lumaframevr-desktop-installer.run
./lumaframevr-desktop-installer.run --ui
```

You can also double-click the executable in a file manager that supports running
scripts. Graphical dialogs use kdialog or zenity when a desktop display and one of
those programs are available; otherwise the installer uses plain terminal prompts.
The self-extracting file includes the payload and Steam helper; no build is needed.
The checksum detects altered downloads, but does not authenticate an untrusted
release publisher.

For terminal use or over SSH, after checksum verification:

```sh
./lumaframevr-desktop-installer.run --install --no-steam
```

Add `--no-ui --yes` for unattended terminal installation. Steam integration is
asked about by default; `--no-steam` skips it, and `--steam` requests it explicitly.
No GitHub destination is implied here; use the maintainer's actual release page.

### When the `.run` cannot execute

Some targets, the Steam Frame included, cannot execute the self-extracting `.run`
directly: the file may arrive without its execute bit, or the filesystem may be mounted
`noexec`. Execute `install-lumaframevr.desktop`, which the directory bundle ships beside
`install.sh` for exactly this case, or hand the `.run` to `sh` as an argument, which never
needs the file itself to be executable:

```sh
sh lumaframevr-desktop-installer-<version>-<build>.run --install --no-steam --yes
sh lumaframevr-desktop-installer-<version>-<build>.run --plan
```

Every installer action and option passes straight through.

## Directory bundle and source-tree route

In an optional directory bundle, verify its files and run the shell installer:

```sh
sha256sum -c SHA256SUMS.txt
./install.sh --ui
./install.sh --install --no-steam --no-ui --yes
```

The bundle also has a double-clickable `install-lumaframevr.desktop` entry. These
commands are alternatives, not steps requiring two installations. From a source
checkout, provide the separately downloaded payload explicitly:

```sh
  ./lumaframevr-desktop-installer-<version>-<build>.run --install --no-steam --yes
```

The payload is `lumaframevr-steam-frame-arm64.tar.gz`, with a `.sha256`
companion. Verify that companion in the payload's folder before source-tree use.
Use the built bundle or self-extracting file for a pinned outer payload checksum.

## Actions and options

The same actions work with `./install.sh` in a bundle or the downloaded executable:

| Action | Effect |
| --- | --- |
| `--install` | Install (default); repeat installation refreshes the installed release. |
| `--repair` | Restore missing registration, launcher or icon; damaged release files need an available payload. |
| `--uninstall` | Remove the application and registration, keeping saved settings and thumbnails. |
| `--uninstall --purge` | Also remove saved settings and thumbnails. |
| `--steam-add` | Add the installed launcher as a Steam non-Steam game. |
| `--steam-remove` | Remove that shortcut. |
| `--plan` | Print the proposed layout without installing the application. |

Options: `--ui` / `--no-ui`, `--steam` / `--no-steam`, `--yes`,
`--prefix DIR` (default `~/.local`), `--payload FILE`, `--add-to-path`,
`--help` (`-h`). `--purge` and `--keep-steam` apply to uninstall; the latter
leaves the shortcut. Use the same `--prefix` when repairing or removing a
custom-prefix installation.
`~/.local/bin` is not on the default PATH of Arch-based systems such as SteamOS.
The installer prints the line to add when it is missing, and `--add-to-path`
writes it into `~/.profile`; uninstall removes that line again.

```sh
./install.sh --plan
./install.sh --repair
./install.sh --steam-add
./install.sh --steam-remove
./install.sh --uninstall
./install.sh --uninstall --purge
./install.sh --help
```

Run only the action you need. Uninstall normally removes the Steam shortcut if
one exists. The Steam helper backs up the binary shortcuts file and preserves
unrelated entries. Exit Steam before editing shortcuts, because Steam rewrites
that file from memory when it shuts down.

## Per-user layout

With the default prefix:

```text
~/.local/opt/lumaframevr/releases/<version>/             payload
~/.local/opt/lumaframevr/current                    release symlink
~/.local/bin/lumaframevr                           launcher
~/.local/share/applications/lumaframevr.desktop
~/.local/share/icons/hicolor/512x512/apps/lumaframevr.png
~/.local/share/lumaframevr/install-record.txt        created paths
~/.local/state/lumaframevr/installer.log             installer log
```

Application settings in `~/.config/lumaframevr` and thumbnails in
`~/.cache/lumaframevr` survive plain uninstall. XDG configuration, cache and state
home overrides are respected. The state/log location is independent of a custom
installation prefix. A foreign release directory is refused; a checksum failure,
insufficient space or a system prefix such as `/usr` prevents installation or
cleans up incomplete release files. The installer may create its state directory
even for a plan or failed attempt; it is not a filesystem-wide no-write promise.

## Launch environment and diagnostics

`launch.sh` defines the whole runtime environment: `DISABLE_GAMESCOPE_WSI=1`,
`XR_RUNTIME_JSON` (defaulting to `/opt/steamvr/steamxr_linuxarm64.json` when that
file exists), `VR_PATHREG_OVERRIDE`, `VR_LOG_PATH`, `ulimit -c 0`,
`LD_LIBRARY_PATH` (the packaged `runtime/` and
`/opt/steamvr/bin/linuxarm64`) and `LUMAFRAMEVR_PACKAGE_ROOT`. The application
menu, an optional Steam shortcut, a direct `launch.sh` call and a terminal start
therefore use the same setup. Prefer `~/.local/bin/lumaframevr` for installed
terminal playback.

Inspect `~/.local/state/lumaframevr/installer.log` and `startup.log` for failures.
Host filesystem tests do not establish native headset acceptance; see
[Known issues](KNOWN-ISSUES.md).
Interface languages ship under `opt/lumaframevr/current/lang/`; see
[Localization](LOCALIZATION.md).
