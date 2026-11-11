# brave-history-mcp: Developer Reference

User installation, configuration and compatibility status are in the
[README](../README.md). This reference preserves implementation details,
alternative transport schemas and validation commands. A client configuration
example is not a claim of verified July/OAuth interoperability.

Current HTTP source builds use local slimr and libhv overlays. The pinned
published registry baseline predates these HTTP changes; do not claim that
baseline builds the current HTTP app without overrides. CI configuration is
not evidence that a runner/platform/client has passed.

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

### Claude Code HTTP schema (July acceptance unverified)

Cursor uses the same URL entry without `"type": "http"`; its July support is deferred.

Point MCP clients at the local service endpoint:

```json
{
  "mcpServers": {
    "brave-history": {
      "type": "http",
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

## Build & Test

```sh
just test       # Debug + ASan/UBSan unit, HTTP, CLI & concurrency tests
just build      # Release build (-O3 -flto) -> build/bin/brave-history-mcp
just lint       # clang-tidy (${CLANG_TIDY:-clang-tidy})
just test-mcp   # MCP 2026-07-28 stdio + ephemeral HTTP conformance test
```
