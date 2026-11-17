# brave-history-mcp

Search, list, and inspect Brave Browser desktop history over MCP using atomic read-only SQLite snapshots.

> **Client compatibility:** MCP **2026-07-28 only**; legacy clients cannot connect.

## Install

```bash
brew tap bdombro/brave-history-mcp https://github.com/bdombro/brave-history-mcp
brew install bdombro/brave-history-mcp/brave-history-mcp
```

Reading `~/Library/Application Support/BraveSoftware/Brave-Browser/` on macOS may require **Full Disk Access** or **App Data** permission for your terminal or `$(brew --prefix)/opt/brave-history-mcp/bin/brave-history-mcp`.

## Usage

Start the Homebrew service:

```bash
brew services start brave-history-mcp
brew services info brave-history-mcp
```

The HTTP server binds `127.0.0.1:10003` by default (pass `--host 0.0.0.0` to expose; `http://127.0.0.1:10003/mcp` locally), stores snapshot caches under `$(brew --prefix)/var/brave-history-mcp`, logs to `$(brew --prefix)/var/log/brave-history-mcp.log`, and loads optional service environment variables from `$(brew --prefix)/etc/brave-history-mcp/env` (`chmod 600`).

Add to `.mcp.json` or OpenCode `opencode.jsonc`:

```json
{
  "mcpServers": {
    "brave-history-mcp": {
      "type": "http",
      "url": "http://127.0.0.1:10003/mcp"
    }
  }
}
```

Or call tools directly over HTTP (enabled by default on `/tools/<name>`):

```bash
curl -sS -X POST http://127.0.0.1:10003/tools/status -d '{}'
curl -sS -X POST http://127.0.0.1:10003/tools/search -d '{"words":["kodiaq","service"]}'
curl -sS -X POST http://127.0.0.1:10003/tools/list -d '{"limit":10}'
curl -sS -X POST http://127.0.0.1:10003/tools/visits -d '{"url":"https://example.com/page"}'
```

Query strings and URL fragments are stripped by default unless `fullUrls: true` is passed.

### Tools

- `search` — Find pages matching all search words (case-insensitive across URL and title), optionally bounded by `from` / `to`
- `list` — List recently visited pages in reverse chronological order, optionally bounded by `from` / `to`
- `visits` — List individual visits for a specific URL (stripping `?query` and `#fragment` by default unless `fullUrls: true`)
- `status` — Show app version

## Environment Variables

| Name | Required | Default | Purpose |
| --- | --- | --- | --- |
| `BRAVE_HISTORY_ROOT` | Optional | macOS Brave User Data dir | Override Brave profile root directory |
| `BRAVE_HISTORY_CACHE` | Optional | `$(brew --prefix)/var/brave-history-mcp` | Override atomic snapshot cache directory |
| `TZ` | Optional | System timezone | Override timezone for timestamp formatting and date queries |
| `BUGSINK_DSN` | Optional | Unset (disabled) | Error reporting DSN; unset disables reporting |

For the background service, put overrides in `$(brew --prefix)/etc/brave-history-mcp/env` (`KEY=VALUE`, `chmod 600`).

## Development

See [docs/contributing.md](docs/contributing.md) for local builds, testing, and contributing.
