# AGENTS.md

Guidance for AI agents working in `brave-history-mcp`.

> Keep this file in sync with code, tooling, and conventions.


## Documentation Upkeep

When a change conflicts with `README.md`, update the affected README and its
linked user/developer documentation in the same change. Keep examples,
configuration, install/build commands and compatibility claims consistent
with implemented behavior; do not claim unverified release or client support.

## Architecture

- Standalone C++26 MCP server and CLI built on `slimr-mcp[http]`, `slimr-sqlite`, `slimr-json`, `slimr-reflect`, `slimr-time`, `slimr-bugsink`, and `slimr-test`.
- `src/app.hpp` defines `make_brave_history_app()` returning `slimr::PluginApp` with `search`, `list`, `visits`, and `status` tools.
- `src/main.cpp` registers Streamable HTTP via `slimr::enable_http(app, slimr::HttpServerOptions{.host = "127.0.0.1", .port = 8083})` and dispatches `app.run(argc, argv)`.
- `src/snapshot.hpp`, `src/db.hpp`, and `src/time.hpp` implement atomic `flock` SQLite snapshotting and thread-safe snapshot/env access (`cache_mutex()`, `env_mutex()`, `withCachedHistoryDb()`).
- `tests/test_main.cpp` and `tests/fixture.hpp` run unit, MCP `2026-07-28`, Streamable HTTP, CLI, and concurrent reader tests against synthetic Brave SQLite fixtures.

## Build, Lint & Test

- `just test` — Debug build with `-fsanitize=address,undefined` and `ctest`
- `just build` — Release build (`-O3 -flto`) outputting `build/bin/brave-history-mcp`
- `just lint` — Runs `"${CLANG_TIDY:-clang-tidy}" -p build/cmake/Debug src/main.cpp tests/test_main.cpp`
- `just test-mcp` — Runs `scripts/test-mcp.mjs` verifying both `mcp` (stdio) and ephemeral `http` (`server/discover` + `tools/list`) under MCP `2026-07-28`

## Packaging & Distribution

- `Formula/brave-history-mcp.rb` — Homebrew formula installing `brave-history-mcp` and configuring `service do` on `127.0.0.1:8083`.
- `scripts/dev-formula.sh` — Local tarball staging helper for `just install-local` / `just reinstall-local` (restores `Formula/brave-history-mcp.rb` on failure).
- `.cursor-plugin/plugin.json`, `.claude-plugin/plugin.json`, `.claude-plugin/marketplace.json` — HTTP plugin manifests pointing to `http://127.0.0.1:8083/mcp`.

## Conventions

- Keep dependencies minimal and package-local (`vcpkg.json` + `CMakeLists.txt`).
- Never access live `~/.local/share/brave-history` or user Brave Browser profiles in tests; use `tests/fixture.hpp`.
- Preserve tool names (`search`, `list`, `visits`, `status`), parameter schemas, URL query/fragment stripping defaults, and C++ memory-safety guarantees.
