# Changelog

## Unreleased

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
