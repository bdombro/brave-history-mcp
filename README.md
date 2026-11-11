# brave-history-mcp

Standalone C++26 MCP server and CLI for searching, listing, and inspecting Brave Browser desktop history via atomic read-only SQLite snapshots.

Each profile's `History` SQLite database is snapshotted atomically (`flock` + copy + `-wal`/`-shm` companion files into `BRAVE_HISTORY_CACHE`, refreshed when source mtime/size changes) and queried read-only without blocking Brave. Query strings and URL fragments are stripped by default unless `fullUrls: true` is passed.

> **Client compatibility:** MCP **2026-07-28 only**; legacy clients cannot connect.
> The workspace has been exercised with OpenCode v2.2.6 using explicit July
> configuration. Cursor's July support is deferred; Claude Code July runtime
> acceptance is not yet verified. Shipping a plugin manifest is not proof of
> client compatibility.

## Try It

After setup, ask your agent:

> Find the pages I visited about Kodiaq servicing last week.

## Features

- Search page titles/URLs, list recent pages and inspect visit history.
- Read atomic snapshots without modifying Brave's live history.
- Hide URL queries and fragments by default; opt in explicitly to full URLs.


## Installation

- **Prebuilt release:** the checked-in [Homebrew formula](Formula/brave-history-mcp.rb)
  currently has a placeholder checksum. It is not a ready-to-install release.
  Check [releases](https://github.com/bdombro/brave-history-mcp/releases) for published
  binaries and a finalized formula before using a release installation command.
- **Build from source:** the current path is [Development](#development).
  Local slimr overlays are for development, not a runtime requirement.
- **Already installed:** follow the setup and service steps below. Installing a
  Cursor/Claude manifest only configures an endpoint; it does not install or
  start the native service.


## Configure Brave Access

Install Brave if needed with `brew install --cask brave-browser`.
The server uses the default Brave desktop profile location; override it with
`BRAVE_HISTORY_ROOT` when needed. macOS privacy controls may require App Data
or Full Disk Access for the terminal or service binary reading your profile.
Grant only the access needed for your chosen deployment.


## Start and Manage the Service

For an already-installed Homebrew formula:

```bash
brew services start bdombro/brave-history-mcp/brave-history-mcp
brew services info brave-history-mcp
brew services restart bdombro/brave-history-mcp/brave-history-mcp
brew services stop bdombro/brave-history-mcp/brave-history-mcp
```

The endpoint is `http://127.0.0.1:8083/mcp`; logs are under
`/opt/homebrew/var/log/`. To run in the foreground instead, use
`brave-history-mcp http` (or `./build/bin/brave-history-mcp http` from a source checkout).
Do not run the foreground server and service on the same port.
Keep the default loopback bind; this is not a shared remote-host deployment.


## Connect Your Agent

After starting the service, add this server to OpenCode v2.2.6's
`opencode.jsonc` (merge it with any existing servers):

```json
{
  "mcp": {
    "servers": {
      "brave-history-mcp": {
        "type": "remote",
        "url": "http://127.0.0.1:8083/mcp",
        "protocol": "2026-07-28"
      }
    }
  }
}
```

Use either an explicit server entry or a plugin manifest, not both.
Alternative client schemas and stdio configuration are in the
[developer reference](docs/development.md). The compatibility notice above
applies to every transport.


## Runtime Requirements

- Brave Browser desktop profiles installed. macOS is the verified host;
  Linux support remains unverified.

On macOS, reading `~/Library/Application Support/BraveSoftware/Brave-Browser/` from a terminal or background `launchd` service (`brew services`) requires **App Data** / **Full Disk Access** permission for the terminal or `brave-history-mcp` binary if protected by macOS privacy controls.

## Tools

- `search` — Find pages matching all search words (case-insensitive across URL and title), optionally bounded by `from` / `to`
- `list` — List recently visited pages in reverse chronological order, optionally bounded by `from` / `to`
- `visits` — List individual visits for a specific URL (stripping `?query` and `#fragment` automatically when querying and displaying, unless `fullUrls: true`)
- `status` — Show app version

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

## Troubleshooting

- **Client cannot connect:** confirm it supports July MCP, the service is
  running, and the client URL matches the configured port. OpenCode needs
  `"protocol": "2026-07-28"`; old `initialize` clients will fail.
- **Port occupied:** inspect the listener and stop only the service/process
  you own, or deliberately choose another port and update the client URL.
- **Works in a terminal, not as a service:** launchd does not inherit
  `~/.zshrc`. Configure the service environment separately or use the
  plugin's supported private configuration file; inspect
  `/opt/homebrew/var/log/` for errors.
- **History unavailable:** check the Brave profile path and macOS privacy permissions; do not copy or modify the live database manually.
- **Source build cannot find vcpkg or slimr ports:** load the development
  environment, bootstrap vcpkg, and register both slimr links and the pinned
  libhv overlay. Never repair this by editing installed headers.
- **Compiler/hardening check fails:** upgrade the Xcode toolchain; set
  `CXX` before first configure. CMake caches the compiler, so an intentional
  compiler change needs a fresh configuration tree.


## Development

This section is for building or changing the server, not configuring an
installed binary. [Architecture, transport reference and test details](docs/development.md)
are separate from the user setup flow.

<details>
<summary>macOS source-build setup (vcpkg and local slimr overlays)</summary>

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

</details>

After the source build succeeds, stage and install the local Homebrew formula:

```bash
just install-local
```

Installing a formula and starting its service are separate actions.
For a source-only foreground run, use
`./build/bin/brave-history-mcp http`.
