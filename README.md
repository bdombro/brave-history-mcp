# brave-history-mcp

```
   .-----------------.
  | HISTORY: FOUND! |
  |  --->  *  <---  |
   '----------------'
```

Use this MCP to find the page you remember from your Brave history. Search titles and URLs, filter by domain or date, and inspect visit timelines across your browser profiles.

Brave keeps its history database busy while the browser is open. This MCP reads private snapshots instead, opens them read-only, and strips URL query strings by default to avoid exposing tokens and tracking parameters.

## Features

- Search and browse visits across Brave profiles with domain and date filters.
- Inspect when and how a page was visited; optionally include full URLs when needed.
- Strictly read-only, with private snapshots that avoid locking Brave's live database.
- Small footprint: snapshots are refreshed on demand; no background indexer or Node/V8 runtime.

## `.mcp.json` Configuration

In the `assistant2` workspace, `brave-history-mcp` is mounted into the unified `build/bin/assistant-mcp` gateway under the `brave_` prefix (`brave_search`, `brave_list`, `brave_visits`, `brave_status`).

To run `brave-history-mcp` as a standalone MCP server in `.mcp.json`:

```json
{
  "mcpServers": {
    "brave-history-mcp": {
      "command": "/absolute/path/to/build/bin/brave-history-mcp",
      "args": ["mcp"],
      "env": {
        "BRAVE_HISTORY_TZ": "Europe/Dublin"
      }
    }
  }
}
```

## App Architecture

The standalone MCP and JSON CLI are implemented in C++26 on `slimr-cpp`.

```text
plugins/brave-history-mcp/
├── vcpkg.json                # Independently resolved dependencies
├── CMakeLists.txt            # App/test targets and transitive dependency linking
├── justfile                  # Thin CMake/Ninja orchestration
├── README.md                 # User & operator documentation
├── AGENTS.md                 # Maintainer & AI agent conventions
├── src/
│   ├── schemas.hpp           # Reflected input structs (`SearchInput`, `ListInput`, `VisitsInput`, `StatusInput`)
│   ├── time.hpp              # Chromium 1601-01-01 microsecond epoch <-> Europe/Dublin ISO-8601 & civil day bounds
│   ├── snapshot.hpp          # Atomic `mkdtemp` staging (`0700` dir, `0600` files) and mtime-cached `History` copy
│   ├── db.hpp                # Read-only SQLite queries (`urls`, `visits`, transition decoding, URL query stripping)
│   ├── app.hpp               # `brave_history::make_brave_history_app()` registering all 4 tools & Bugsink DSN
│   └── main.cpp              # Standalone CLI & stdio MCP entrypoint
├── tests/
│   └── test_main.cpp         # 28 test cases (103 assertions) against synthetic Chromium `History` SQLite DBs
└── build/bin/
    └── brave-history-mcp     # Standalone release binary
```

### Libraries Used

- **`<mcp.hpp>` & `<reflect.hpp>`:** Typed MCP tool definitions, compile-time JSON Schema generation, and narrow JSON CLI.
- **`<sqlite.hpp>` (vcpkg static SQLite):** Read-only SQLite connection (`SQLITE_OPEN_READONLY` + `PRAGMA query_only = ON`) and statement caching.
- **`<json.hpp>` (`glaze`):** Fast JSON serialization.
- **`<bugsink.hpp>` (libcurl):** opt-in error reporting through `BUGSINK_DSN`.
- **`<test.hpp>` (`doctest`):** Temp-dir Chromium SQLite fixture testing with `-fsanitize=address,undefined`.

## How It Reads History Safely

1. **Zero locking conflict with Brave:** Brave holds an exclusive lock on its `History` SQLite database while running. `brave-history` never opens Brave's live database directly.
2. **Atomic mtime-cached snapshots (`src/snapshot.hpp`):** Whenever a profile's `History` (or its `-wal` / `-journal` sidecar) has a newer `mtime` than the cached snapshot in `~/.local/share/brave-history/<profile>/`, it stages a copy in a `0700` temporary directory (`mkdtemp`), sets file permissions to `0600`, and moves the snapshot into place.
3. **Read-only SQLite connections:** Queries open the cached `History` snapshot with `SQLITE_OPEN_READONLY` and `PRAGMA query_only = ON`.
4. **Privacy-by-default URL sanitization:** Returned URLs automatically strip `?query` strings and `#fragment` identifiers (which frequently contain OAuth tokens, session IDs, or tracking parameters). Pass `"fullUrls": true` when the full query string is needed. Keyword search still matches against the full stored URL.
5. **Chromium epoch & timezone conversion (`src/time.hpp`):** Converts Chromium's 1601-01-01 UTC microsecond timestamps to ISO-8601 strings in `Europe/Dublin` local time with UTC offset (e.g. `2026-07-01T13:00:00+01:00`). Date filters (`from`, `to`) are inclusive local calendar days (`YYYY-MM-DD`).

## Tools & CLI Commands

Every tool is exposed over MCP (`brave-history-mcp mcp` or via `assistant-mcp` under the `brave_` prefix) and on the CLI (`brave-history-mcp <tool> ['{"arg":"val"}']`).

| Tool | Gateway Name | Description |
| --- | --- | --- |
| `search` | `brave_search` | Keyword search over page titles and URLs. Every word in `query` must match (case-insensitive substring). Supports `domain`, `from`, `to`, `profile`, `limit` (default 25, max 500), and `fullUrls`. Returns title, URL, last visit time, visit count, and profile, newest first. |
| `list` | `brave_list` | List recently visited pages, newest first, with optional `domain`, `from`, `to`, `profile`, `limit`, and `fullUrls` filters. |
| `visits` | `brave_visits` | Detailed visit log for an exact `url` (or URL prefix with `"prefix": true`), returning visit timestamp, Chromium transition type (`link`, `typed`, `reload`, `form_submit`, …), visit duration, and profile. |
| `status` | `brave_status` | Returns plugin version, discovered profiles, and active paths. |

### Domain Filtering

`"domain": "amazon.co.uk"` matches both `amazon.co.uk` and any subdomain (`www.amazon.co.uk`, `smile.amazon.co.uk`), while rejecting lookalike domains (`notamazon.co.uk`).

### CLI Examples

```bash
# Search history for keywords within a domain and date range
./build/bin/brave-history-mcp search '{"query":["kodiaq","service"],"domain":"skoda.ie","from":"2026-01-01"}'

# List 10 most recent visits across all profiles
./build/bin/brave-history-mcp list '{"limit":10}'

# Inspect individual visits under a URL prefix
./build/bin/brave-history-mcp visits '{"url":"https://github.com/bdombro","prefix":true,"limit":20}'

# Check status
./build/bin/brave-history-mcp status
```

## Environment Variables

| Variable | Default | Purpose |
| --- | --- | --- |
| `BRAVE_HISTORY_ROOT` | `~/Library/Application Support/BraveSoftware/Brave-Browser` | Brave user data directory containing `Default` and `Profile *` folders; set explicitly on Linux |
| `BRAVE_HISTORY_CACHE` | `~/.local/share/brave-history` | Owner-only (`0700`) directory for staged `History` snapshots |
| `BRAVE_HISTORY_TZ` | `Europe/Dublin` | IANA timezone used for local `from`/`to` day bounds and ISO-8601 output |
| `BUGSINK_DSN` | unset (disabled) | Error reporting DSN (`""` disables) |

> **macOS permissions:** The host terminal or MCP client (Claude Code, Cursor, opencode) needs permission to read `~/Library/Application Support/BraveSoftware/` (granted via macOS App Data prompt or Full Disk Access).

## Building & Testing

Build with Clang/C++26 and a libc++ supporting FAST hardening, CMake, Ninja, [just](https://just.systems), and [vcpkg](https://vcpkg.io). Set `VCPKG_ROOT` to a bootstrapped checkout and `VCPKG_OVERLAY_PORTS` to your environment's local overlay directory. Linux/macOS use native triplets; support must be verified by each platform's CI, not inferred from host detection.

`just build`, `just test`, and `just lint` automatically refresh dependencies under `build/deps/` before configuring CMake. Debug sanitizer and Release build graphs live under `build/cmake/`; Ninja skips unchanged compilation. Consumers use installed slimr CMake targets, never source-checkout include overrides. SQLite, including FTS5, is statically linked: the runtime host needs no SQLite CLI, server, or separate SQLite library.

Until versioned registry publication is authorized, the local overlay must provide the `slimr-*` ports. Refreshing dependencies installs local edits before compilation. No binaries are installed globally.

```bash
just test     # Run 28 test cases (103 assertions) against temp SQLite History fixtures with ASan + UBSan
just build    # Build/test optimized binary at build/bin/brave-history-mcp
just lint     # clang-tidy using the CMake compilation database
just test-mcp # Pinned Inspector: isolated initialize + tools/list (no real tool calls)
```

### Pull request checks

The workflow runs sanitizer tests, optimized tests/builds, clang-tidy, and isolated Inspector discovery on Linux/macOS. Configure `VCPKG_REF` as the tested full vcpkg commit SHA; slimr packages use the pinned public registry without a dependency token. Local workflow validation does not establish that remote checks have passed; enable required `checks (ubuntu-24.04)` and `checks (macos-15)` branch checks only after actual successful runs.
