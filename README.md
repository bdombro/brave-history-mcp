# brave-history-mcp

Standalone C++26 MCP server and CLI for searching, listing, and inspecting Brave Browser desktop history via atomic read-only SQLite snapshots.

Each profile's `History` SQLite database is snapshotted atomically (`flock` + copy + `-wal`/`-shm` companion files into `BRAVE_HISTORY_CACHE`, refreshed when source mtime/size changes) and queried read-only without blocking Brave. Query strings and URL fragments are stripped by default unless `fullUrls: true` is passed.

## Requirements

- macOS or Linux with Brave Browser desktop profiles installed
- C++26 toolchain, CMake, Ninja, `vcpkg`, and Node.js (for `just test-mcp`)

On macOS, reading `~/Library/Application Support/BraveSoftware/Brave-Browser/` from a terminal or background `launchd` service (`brew services`) requires **App Data** / **Full Disk Access** permission for the terminal or `brave-history-mcp` binary if protected by macOS privacy controls.

## Tools

- `search` — Find pages matching all search words (case-insensitive across URL and title), optionally bounded by `from` / `to`
- `list` — List recently visited pages in reverse chronological order, optionally bounded by `from` / `to`
- `visits` — List individual visits for a specific URL (stripping `?query` and `#fragment` automatically when querying and displaying, unless `fullUrls: true`)
- `status` — Show app version

## Transports & July `2026-07-28` MCP Breaking Change

`brave-history-mcp` supports:

- **Streamable HTTP** (recommended for IDE/agent clients): `brave-history-mcp http` (default `http://127.0.0.1:8083/mcp`, loopback-only unless `--allow-non-loopback`, fixed port `8083` unless `--allow-ephemeral-port`)
- **Stdio MCP**: `brave-history-mcp mcp` (newline-delimited JSON-RPC over stdin/stdout)
- **Direct CLI**: `brave-history-mcp <tool> ['{"arg":"val"}']` (defaults to `{}`)

This server targets **MCP protocol version `2026-07-28`**:

- Legacy `initialize` is removed and returns JSON-RPC error `-32601` (HTTP `404`).
- Clients must send `_meta["io.modelcontextprotocol/protocolVersion"] = "2026-07-28"` and `_meta["io.modelcontextprotocol/clientCapabilities"]` in `params`, and use `server/discover` instead of `initialize`.
- Streamable HTTP `POST /mcp` requests must include `Host` (loopback), `Accept: application/json, text/event-stream`, `Content-Type: application/json`, `MCP-Protocol-Version: 2026-07-28`, `Mcp-Method: <method>`, and `Mcp-Name: <tool>` on `tools/call`.

## Client Configuration

### Cursor / Claude Code (Streamable HTTP)

Point MCP clients at the local service endpoint:

```json
{
  "mcpServers": {
    "brave-history": {
      "url": "http://127.0.0.1:8083/mcp"
    }
  }
}
```

### Stdio

```json
{
  "mcpServers": {
    "brave-history": {
      "command": "brave-history-mcp",
      "args": ["mcp"]
    }
  }
}
```

### Plugin Manifests

Preconfigured HTTP plugin manifests targeting `http://127.0.0.1:8083/mcp` live under:

- `.cursor-plugin/plugin.json`
- `.claude-plugin/plugin.json`
- `.claude-plugin/marketplace.json`

## Homebrew & Background Service (`127.0.0.1:8083`)

`Formula/brave-history-mcp.rb` builds and installs `brave-history-mcp` natively with CMake, Ninja, and `vcpkg`, and registers a `launchd` / `brew services` unit running `brave-history-mcp http --host 127.0.0.1 --port 8083` (`keep_alive true`, logs in `$(brew --prefix)/var/log/brave-history-mcp.log`).

Local lifecycle recipes in `justfile`:

- `just install` — Install from the formula
- `just install-local` / `just reinstall-local` — Stage and install from the local checkout via `scripts/dev-formula.sh` (restoring the formula on failure)
- `just start` / `just stop` / `just restart` / `just status` — Manage `brew services`
- `just uninstall` — Stop the service and uninstall

## CLI Usage

```sh
brave-history-mcp status
brave-history-mcp search '{"words":["kodiaq","service"]}'
brave-history-mcp list '{"limit":10}'
brave-history-mcp visits '{"url":"https://example.com/page"}'
brave-history-mcp http --host 127.0.0.1 --port 8083
brave-history-mcp mcp
```

## Configuration

| Env Var | Default | Purpose |
| --- | --- | --- |
| `BRAVE_HISTORY_ROOT` | OS Brave User Data dir | Override Brave profile root directory |
| `BRAVE_HISTORY_CACHE` | OS cache dir + `/brave-history-mcp` | Override atomic snapshot cache directory |
| `TZ` | System timezone | Override timezone used for timestamp formatting and date queries |
| `BUGSINK_DSN` | Unset (disabled) | Optional error reporting endpoint |

## macOS Build Prerequisites (vcpkg setup)

Install [Homebrew](https://brew.sh) and current Xcode Command Line Tools
(`xcode-select --install`; finish installation before continuing), or full Xcode.
Apple Clang 21 is the verified C++26/libc++ toolchain; older installations need
upgrading. These instructions assume Apple Silicon Homebrew at `/opt/homebrew`.

Add this environment snippet to `~/.zshrc`, then open a new terminal (or run
`source ~/.zshrc`). Use `~/.zshrc`, not `~/.zshenv`, to avoid changing
non-interactive zsh scripts:

`local-ports` is the dedicated vcpkg overlay directory for local development
linking: `just publish --local --all` registers symlinks to your slimr-cpp checkout,
overriding published packages without making a registry release. See the
[publishing guide on slimr-cpp](https://github.com/bdombro/slimr-cpp/blob/main/docs/publishing.md#local-environment).

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
export VCPKG_OVERLAY_PORTS="$VCPKG_ROOT/local-ports"
# Use your existing workspace checkout instead when available.
export SLIMR_CPP="${SLIMR_CPP:-$HOME/dev/bdombro/slimr-cpp}"
export CXX="/usr/bin/clang++"
export CLANG_TIDY="/opt/homebrew/opt/llvm/bin/clang-tidy"
export PATH="$VCPKG_ROOT:/opt/homebrew/opt/ruby/bin:/opt/homebrew/bin:$PATH"
```

Then install and build from the repository root:

```bash
brew install cmake ninja just llvm node ruby autoconf automake libtool pkgconf
mkdir -p "$(dirname "$VCPKG_ROOT")"
if [ ! -d "$VCPKG_ROOT/.git" ]; then
  git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
fi
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
mkdir -p "$(dirname "$SLIMR_CPP")"
if [ ! -d "$SLIMR_CPP/.git" ]; then
  git clone https://github.com/bdombro/slimr-cpp.git "$SLIMR_CPP"
fi
(cd "$SLIMR_CPP" && just publish --local --all)
ln -s "$SLIMR_CPP/ports/libhv" "$VCPKG_OVERLAY_PORTS/libhv"

"$CXX" --version
vcpkg version
BUGSINK_DSN="" just test
just build
```

The libhv link is a one-time step: if it exists, verify that it points to the
chosen checkout's `ports/libhv` before skipping the command. Never overwrite
a conflicting port directory or modify installed headers/caches. Current HTTP
builds need both local slimr overlays and the separately pinned libhv port;
the older published registry baseline alone is not an HTTP build substitute.
`just deps` installs dependencies under `build/deps/` for the native triplet
(`arm64-osx` or `x64-osx`). SQLite with FTS5 and curl come from vcpkg, not
macOS's system SQLite or separate Homebrew library installs.

Bootstrapping puts the executable at `$VCPKG_ROOT/vcpkg`, with no `bin/`
subdirectory. Keg-only Homebrew Ruby supplies `/opt/homebrew/opt/ruby/bin/ruby`.
Homebrew LLVM supplies lint tooling; select a different compatible `CXX`
before first configure if needed, since CMake caches it. For reproducible
vcpkg tooling, check out the tested full `VCPKG_REF` commit before bootstrapping;
package registry baselines pin resolution separately.
For live history, install Brave separately with `brew install --cask brave-browser`
and grant the permissions described above. Tests use synthetic history and
these commands do not start services or access your live browser profile.

## Build & Test

```sh
just test       # Debug + ASan/UBSan unit, HTTP, CLI & concurrency tests
just build      # Release build (-O3 -flto) -> build/bin/brave-history-mcp
just lint       # clang-tidy (${CLANG_TIDY:-clang-tidy})
just test-mcp   # MCP 2026-07-28 stdio + ephemeral HTTP conformance test
```
