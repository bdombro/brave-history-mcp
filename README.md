# brave-history

Search Brave browser history (titles, URLs, visits) over MCP. Read-only.

A Claude Code / Cursor plugin that runs a small MCP server (`node dist/brave-history.mjs mcp`). It reads Brave's Chromium `History` SQLite DB for every profile (`Default`, `Profile N`) on macOS.

## How it reads history

- Brave locks `History` while running, so the server never opens it. When the original (or its `-wal` / `-journal` sidecar) is newer than the cached copy, it copies them to `~/.local/share/brave-history/<profile>/`. It queries that copy read-only with `node:sqlite`.
- Brave's own files are never written. There is no index of its own and no background watcher; the copy refreshes when a command runs.
- Times are returned as ISO 8601 in Europe/Dublin local time with an offset (e.g. `2026-07-01T13:00:00+01:00`). Date filters (`--from`, `--to`) are inclusive local days.
- macOS: the host app (terminal, Claude Code, Cursor) needs permission to read `~/Library/Application Support/BraveSoftware/` (Full Disk Access or the app-data prompt).

## Installation

Requires Node >= 22.5 (for `node:sqlite`) on the host's PATH. Nothing is installed at startup.

### Claude Code

```bash
/plugin marketplace add bdombro/brave-history
/plugin install brave-history@brave-history
```

### Cursor

Cursor Dashboard → **Settings → Plugins → Team Marketplaces → Import**, then enter `https://github.com/bdombro/brave-history`. For a local checkout: `just plugin-cursor-upsert`.

## Commands

Each command is also an MCP tool with the same arguments. Run `brave-history <command> --help` for details. Add `--json` on the CLI for JSON output.

- `brave-history search <words...> [--domain D] [--from YYYY-MM-DD] [--to YYYY-MM-DD] [--profile P] [--limit N]`: keyword search over page titles and URLs. Every word must match (case-insensitive substring). Returns title, URL, last visit, visit count and profile, newest first.
- `brave-history list [--domain D] [--from] [--to] [--profile] [--limit]`: recent pages, newest first.
- `brave-history visits <url> [--prefix] [--from] [--to] [--profile] [--limit]`: every visit to one URL, or to every URL under a prefix, with time, transition type (`link`, `typed`, `reload`, …) and duration.
- `brave-history status`: show the version.

`--domain amazon.com` matches `amazon.com` and its subdomains (`www.amazon.com`), not `notamazon.com`. `--limit` defaults to 25 (max 500).

Built-ins: `completion`, `mcp`, `version`.

### Environment overrides

| Variable | Default |
| --- | --- |
| `BRAVE_HISTORY_ROOT` | `~/Library/Application Support/BraveSoftware/Brave-Browser` |
| `BRAVE_HISTORY_CACHE` | `~/.local/share/brave-history` |
| `BRAVE_HISTORY_TZ` | `Europe/Dublin` |

## Development

```bash
just setup
just test            # Biome + tsc + node:test (fixture History DBs in a temp dir)
just run search foo  # from source
just build           # rebuild the committed dist/brave-history.mjs
```

Commit `dist/brave-history.mjs` with source changes. Release with `just release <major|minor|patch>`.
