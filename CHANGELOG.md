# Changelog

## Unreleased

- Move default snapshot cache directory from `~/.local/share/brave-history` to `$(brew --prefix)/var/brave-history-mcp` (`BRAVE_HISTORY_CACHE` override still wins)
- Add `--log-file <path>` stdout/stderr redirection with 10 MiB log rotation
- HTTP server no longer rejects non-loopback hosts, port 0, or foreign Host/Origin headers (slimr-mcp upgrade)
- Internal: align build, CI, Homebrew formula, and documentation with cpp-templates
