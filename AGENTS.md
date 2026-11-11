# AGENTS.md (`brave-history`)

Written for: AI coding agents working in the `brave-history` C++26 plugin repository.

`brave-history` is a strictly read-only C++26 MCP plugin and CLI built on `slimr-cpp` that searches Brave browser history across all profiles (`Default`, `Profile N`) by copying Brave's SQLite `History` (+ `-wal`/`-journal` sidecars) to `~/.local/share/brave-history/<profile>/` and querying the snapshot read-only.

## Commands

| Task | Command |
| --- | --- |
| Run unit & SQLite fixture tests (ASan + UBSan) | `just test` |
| Build release binary (`build/bin/brave-history-mcp`) | `just build` |
| Static analysis | `just lint` |
| Isolated MCP startup/discovery | `just test-mcp` |
| Clean build artifacts | `just clean` |

## Module Boundaries

| Path | Owns | Must Not |
| --- | --- | --- |
| `src/main.cpp` | Thin entrypoint calling `brave_history::make_brave_history_app().run(argc, argv)` | Business logic |
| `src/app.hpp` | `make_brave_history_app()` registering `search`, `list`, `visits`, `status` | Direct SQLite queries |
| `src/schemas.hpp` | Input structs (`SearchInput`, `ListInput`, `VisitsInput`, `StatusInput`) | Runtime logic |
| `src/snapshot.hpp` | Profile discovery (`Default`, `Profile *`), mtime comparison, atomic `mkdtemp` staging (`0700` dir, `0600` files) | Ever open or modify files inside `BRAVE_HISTORY_ROOT` for writing |
| `src/db.hpp` | Read-only SQLite queries (`SQLITE_OPEN_READONLY` + `PRAGMA query_only = ON`), domain/keyword matching, URL query/fragment stripping | Open live Brave DB directly |
| `src/time.hpp` | Chromium epoch (`11644473600000000` us offset from 1601-01-01 to 1970-01-01) and `Europe/Dublin` civil date conversion | Global unsynchronized `TZ` mutation |
| `tests/test_main.cpp` | Synthetic Chromium `History` SQLite fixture tests in `TempDirGuard` | Read real user browser history |

## Safety & App Conventions

1. **Never open Brave's live `History` database directly:** Always copy `History` (and `-wal` / `-journal` if present) into `BRAVE_HISTORY_CACHE/<profile>/` first and open only the snapshot copy with `readonly = true` and `PRAGMA query_only = ON`.
2. **File permissions:** Cache directories must be `0700` and copied SQLite files must be `0600` (tightened on every refresh).
3. **Strip query strings and fragments by default:** URLs returned in results must strip `?query` and `#fragment` unless `fullUrls: true` is explicitly passed.

## Build Architecture

CMake declares targets/dependencies; Ninja owns incremental builds. Just orchestrates automatic `deps` before configure/build/test/lint; CMake manifest auto-install is disabled. Set `VCPKG_ROOT` and `VCPKG_OVERLAY_PORTS` in the environment. Dependencies install under `build/deps/`; Debug sanitizer and Release trees live under `build/cmake/`. Refresh installed slimr headers through dependency installation, never source-checkout include overrides. SQLite is statically linked with FTS5; no system SQLite CLI or server is required. `run` does not rebuild.

Use a Clang/C++26 toolchain with libc++ FAST hardening, CMake, Ninja, vcpkg, just, and compatible clang-tidy. Inspector checks use pinned npm tooling and an isolated server environment; never call real browser/data tools in CI.

## C++ Memory Safety

- Immediately wrap owned C handles, allocations, descriptors, and temporary resources in RAII. Make owning wrappers noncopyable unless copying creates independent ownership; moves must leave the source safely destructible.
- Borrow views, pointers, and references only for the current call by default. Store owning values unless lifetime/invalidation guarantees are explicit. Deferred callbacks require explicit ownership-safe captures; no escaping reference captures without a demonstrated lifetime guarantee.
- Initialize scalar/pointer members at declaration and local values before use. Compiler auto-initialization is defense in depth, not permission for uninitialized reads.
- Check range/overflow before narrowing lengths/counts at C API boundaries; casts alone are not validation. Report invalid inputs using existing error mechanisms.
- Never let C++ exceptions escape C callbacks. Translate them into the C API's error/abort mechanism and propagate failures at the C++ boundary; do not silently swallow them.
- Preserve hardening, zero auto-initialization, ASan/UBSan with fatal reports, and required lint/tests. These checks do not prove memory safety.
