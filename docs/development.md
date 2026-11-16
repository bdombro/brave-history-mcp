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

`brave-history-mcp` targets macOS (`platforms := "macos"`) because it reads desktop Brave Browser profiles. On macOS, builds use Apple Clang (`/usr/bin/clang++`) targeting macOS 15.0+ with no runtime dependency on Homebrew LLVM.

## Transports & MCP `2026-07-28`

- **Streamable HTTP**: `brave-history-mcp http` (default `0.0.0.0:10003`, override with `--host` / `--port`)
- **Stdio MCP**: `brave-history-mcp mcp`
- **Direct CLI**: `brave-history-mcp <tool> ['{"arg":"val"}']` (defaults to `{}`)

This server targets **MCP protocol version `2026-07-28`** (`server/discover` instead of legacy `initialize`).

## Release

Push changes to `main`, add an entry under `## Unreleased` in `CHANGELOG.md`, then run:

```bash
just release patch   # or minor / major
```

## Dropped Tooling

Do not re-add foreground runners, local tap/overlay symlinking, GitHub-branch/local-publish/dry-run helpers, standalone formula checks, or JS formula helpers.
