# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.1] - 2026-11-08

### Added

- `search`: keyword search over Brave history titles and URLs (words ANDed), with domain, date range, profile and limit filters.
- `list`: recent pages, optionally by domain and date range.
- `visits`: every visit to a URL or URL prefix, with transition type and duration.
- Reads each profile's `History` through a cached copy in `~/.local/share/brave-history/<profile>/`, refreshed when the original is newer. Brave's files are never written.
- Overrides: `BRAVE_HISTORY_ROOT`, `BRAVE_HISTORY_CACHE`, `BRAVE_HISTORY_TZ`.

### Removed

- Template demo commands (`echo`, `render-json`, `workspaces`) and the in-memory database.

### Changed

- Requires Node >= 22.5 (`node:sqlite`).
