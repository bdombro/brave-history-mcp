# brave-history-mcp: Developer Reference

User installation and configuration are in [README.md](../README.md).

## Development Loop

```bash
just doctor
just test
just lint
just test-mcp
```

`just lint` uses `LLVM_BIN` (defaults to `$(brew --prefix llvm@23)/bin`) for `clang-format` and `clang-tidy`. Iterate quickly on a single doctest case with `just test "<case>"`. Run the direct binary on a separate dev port:

```bash
./build/bin/brave-history-mcp http --port 20003
```

## Platforms

`brave-history-mcp` targets macOS (`platforms := "macos"`) because it reads desktop Brave Browser profiles. On macOS, builds use Homebrew `llvm@23` (`clang`/`clang++`) with static `libc++` and symbol hiding targeting macOS 15.0+, with no runtime dependency on Homebrew LLVM.

## Transports & MCP `2026-07-28`

- **Streamable HTTP**: `brave-history-mcp http` (default `127.0.0.1:10003`, override with `--host` / `--port`; pass `--host 0.0.0.0` to expose)
- **Direct CLI**: `brave-history-mcp <tool> ['{"arg":"val"}']` (defaults to `{}`)

This server targets **MCP protocol version `2026-07-28`** (`server/discover` instead of legacy `initialize`).

## Develop against an unreleased lib

vcpkg reads extra port dirs from `VCPKG_OVERLAY_PORTS`, and an overlay wins over the registry. Point it at a dir of symlinks, one per unreleased lib; manifests never change:

```sh
mkdir -p ~/.vcpkg/overlays/dev
ln -s ~/dev/slimr/<name>/ports/slimr-<name> ~/.vcpkg/overlays/dev/slimr-<name>
export VCPKG_OVERLAY_PORTS=~/.vcpkg/overlays/dev
just test
```

Remove the symlink (or `unset VCPKG_OVERLAY_PORTS`) to build against the released lib again. Keep the export out of shell startup files. `just release` refuses to run while `VCPKG_OVERLAY_PORTS` is set.

## Release

Push changes to `main`, add an entry under `## Unreleased` in `CHANGELOG.md`, then run:

```bash
just release patch   # or minor / major
```

## Dropped Tooling

Do not re-add foreground runners, local tap/overlay symlinking, GitHub-branch/local-publish/dry-run helpers, standalone formula checks, or JS formula helpers.
