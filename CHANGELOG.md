# Changelog

## Unreleased

## v2.2.2 - 2026-11-17

- feat: align with cpp-templates, enable /tools HTTP surface, and drop per-tool CLI (#2)
  * feat: use VCPKG_OVERLAY_PORTS and guard release
  * chore: adopt cpp-templates CI/CD and contributing docs
  * docs: maintainer setup for release App, secrets, rulesets
  * docs: soft alignment marker and release-bot contributing docs
  * feat: drop per-tool CLI and enable /tools HTTP surface
  Remove allowCliTools and JSON CLI arg branch; update test-mcp.ts and docs for /tools/*. Convert CLI test case to assert server-only CLI flag/tool rejection because per-tool CLI was removed.
  * chore: bump slimr-serve v0.3.1 and slimr-mcp v0.2.2 baselines
- chore: slow gh run watch polling to 30s

- Drop per-tool CLI; expose HTTP tool endpoints at `POST /tools/<name>` (and `GET /tools/openapi.json`, `GET /tools/schema`) by default alongside `/mcp`.

## v2.2.1 - 2026-11-17

- Default HTTP bind is now `127.0.0.1:10003` (pass `--host 0.0.0.0` to expose) and add OpenAPI 3.1 `GET /openapi.json` and `--openapi` via `slimr-serve` v0.3.0 and `slimr-mcp` v0.2.1.
- Internal: sync `scripts/test-mcp.ts` (`/openapi.json` check) and document `overlay-ports` workflow in `docs/development.md`.

## v2.2.0 - 2026-11-16

- Internal: remove temporary vcpkg overlay-ports and pin slimr-serve v0.1.1 and slimr-mcp v0.2.0.
- Internal: migrate to slimr-serve typed handlers and remove stdio mcp mode.
- Internal: drop libhv and slimr-curl; upgrade to slimr-mcp v0.1.3 (cpp-httplib) and slimr-bugsink v0.1.2.
- Internal: switch macOS builds to llvm@23 with static libc++ and symbol hiding.

## v2.1.0 - 2026-11-16

- Wire `VERSION` into `--version`, `status`, and Bugsink release tags
- Bind HTTP server to `0.0.0.0:10003` by default
- Move default snapshot cache directory from `~/.local/share/brave-history` to `$(brew --prefix)/var/brave-history-mcp` (`BRAVE_HISTORY_CACHE` override still wins)
- Add `--log-file <path>` stdout/stderr redirection with 10 MiB log rotation
- HTTP server no longer rejects non-loopback hosts, port 0, or foreign Host/Origin headers (slimr-mcp upgrade)
- Internal: align build, CI, Homebrew formula, and documentation with cpp-templates
- Internal: resolve slimr registries from the `slimr-down-now` org
