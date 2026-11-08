---
id: brave-history
name: brave-history
description: Operates the brave-history CLI (echo, render-json, status, workspaces :id delete, workspaces :id get, and 4 more). Use when the user mentions brave-history, echo, render-json, status, or related tasks.
enabled: true
---

# brave-history

Search Brave browser history (titles, URLs, visits) over MCP. (Zod schemas, typed commands, REST CRUD)

## Execution

Invoke via shell:

```bash
brave-history <subcommand> [options] [args]
```

## Options & Help Discovery

- Run `brave-history <subcommand> --help` to inspect flags, choices, and positional arguments before running unfamiliar subcommands.
- Run `brave-history --help` at the root for top-level options and command routing.

## Commands

- **`brave-history echo`** — Echo a message (MCP-friendly command).
- **`brave-history render-json`** — Echo a JSON message (schema-first JSON command demo).
- **`brave-history status`** — Show app version.
- **`brave-history workspaces :id delete`** — Delete a workspace.
- **`brave-history workspaces :id get`** — Get one workspace.
- **`brave-history workspaces :id patch`** — Patch a workspace name.
- **`brave-history workspaces :id put`** — Replace a workspace.
- **`brave-history workspaces get`** — List workspaces.
- **`brave-history workspaces post`** — Create a workspace.

## Workflow & Pitfalls

- Always run `brave-history <subcommand> --help` instead of guessing options or reading large doc files.
- Pass `--` before arguments that look like flags.
- Pass `--json` when machine-readable structured output is supported.

## Install location

- **Agent plugin:** plugins built with `brave-history mcp bundle` ship this skill.
- **Manual:** copy or symlink `skills/brave-history/` into your agent's skill directory (for example `~/.agents/skills/brave-history/` or `~/.claude/skills/brave-history/`).
