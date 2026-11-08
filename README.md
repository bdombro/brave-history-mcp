# brave-history

Search Brave browser history (titles, URLs, visits) over MCP. (Zod schemas, typed commands, in-repo manifests).

## Overview

`brave-history` packages an MCP server and agent skills directly for Cursor and Claude Code marketplaces:

- **Cursor Plugin**: inline MCP configuration in `.cursor-plugin/plugin.json` runs `node ${CURSOR_PLUGIN_ROOT}/dist/brave-history.mjs mcp`.
- **Claude Code Plugin**: inline MCP configuration in `.claude-plugin/plugin.json` runs `node ${CLAUDE_PLUGIN_ROOT}/dist/brave-history.mjs mcp`.
- **In-repo skills**: `skills/brave-history/SKILL.md` discovered and loaded by agent platforms.
- **Committed Node bundle**: `dist/brave-history.mjs` is self-contained (dependencies and schemas bundled), so a fresh clone runs with no install step.

## Installation

Requires Node >=20 on the host's PATH. Nothing is installed at startup; stdout contains only MCP messages. Maintainers develop, test, and bundle (esbuild) with Node ≥ 22.18, npm, and [just](https://just.systems); no Bun required.

The in-repo example retains its `argsbarg: file:../..` development dependency. Before distributing a standalone copy, resolve that dependency for the destination repository and regenerate its lockfile; the parent-checkout path is not portable.

### Cursor

Recommended: import directly from GitHub — Cursor Dashboard → **Settings → Plugins → Team Marketplaces → Import**, then enter `https://github.com/bdombro/brave-history` (subdirectory `examples/agent-plugin` once copied to its own repo). Once published to the [official marketplace](https://cursor.com/marketplace/publish), install via `/add-plugin brave-history` or the Customize sidebar.

### Claude Code

Recommended: add the GitHub repo as a marketplace, then install:

```bash
/plugin marketplace add <owner>/<repo>
/plugin install brave-history@brave-history
```

The marketplace name comes from `.claude-plugin/marketplace.json`, not the repository name.

Once merged into [anthropics/claude-plugins-official](https://github.com/anthropics/claude-plugins-official), install via `/plugin install brave-history@claude-plugins-official`.

## Commands

- `brave-history echo` — Echo text back to stdout or inspect flags.
- `brave-history render-json` — Process structured JSON payloads with schema validation.
- `brave-history status` — Show application version with a Zod output schema (`--json`).
- `brave-history workspaces` — Manage workspace resources (REST CRUD with in-memory SQLite).

### Built-in commands

- `brave-history completion` — Print shell tab-completion scripts (bash, zsh, fish).
- `brave-history mcp` — Start the Model Context Protocol (stdio) server for AI coding agents.
- `brave-history version` — Display version information.

## Contributing / Local Development

```bash
git clone https://github.com/bdombro/brave-history.git
cd bun-argsbarg/examples/agent-plugin

just setup
just test

# Rebuild the committed Node bundle and run the MCP server from it
just build
node dist/brave-history.mjs mcp

# Link into local Cursor plugins for live testing
just plugin-cursor-upsert

# Register this checkout as a marketplace and install in Claude Code
just plugin-claude-install
```

Restart Claude Code after installing. Use `node src/index.ts <command>` for CLI commands from source. Commit `dist/brave-history.mjs` whenever source changes (`just build`). Release with `just release <major|minor|patch>` (no prompt; commits all changes, tags, pushes and creates the GitHub release).

