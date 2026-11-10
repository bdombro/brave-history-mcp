# AGENTS.md (`brave-history`)

Written for: AI coding agents working in the `brave-history` C++26 plugin repository.

`brave-history` is a strictly read-only C++26 MCP plugin and CLI built on `slimr-cpp` that searches Brave browser history across all profiles (`Default`, `Profile N`) by copying Brave's SQLite `History` (+ `-wal`/`-journal` sidecars) to `~/.local/share/brave-history/<profile>/` and querying the snapshot read-only.

## Commands

| Task | Command |
| --- | --- |
| Run unit & SQLite fixture tests (ASan + UBSan) | `just test` |
| Build release binary (`dist/brave-history`) | `just build` |
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
