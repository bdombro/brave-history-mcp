# bash (not sh); -e bail on errors, -u error on unset vars, pipefail fails pipelines when any stage fails
set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

export PATH := justfile_directory() + "/node_modules/.bin:" + env("PATH")

# List available recipes (default)
_:
    @just --list

# Rebuild the committed Node bundle that plugins run (commit dist/brave-history.mjs)
build:
    esbuild src/index.ts --bundle --platform=node --target=node22 --format=esm --minify-whitespace --minify-syntax --outfile=dist/brave-history.mjs

# Install the Claude Code plugin from this checkout (restart Claude Code afterwards)
plugin-claude-install: build
    claude plugin marketplace add "$(pwd)"
    claude plugin install brave-history@brave-history

# Link the plugin into ~/.cursor/plugins/local for local testing
plugin-cursor-upsert: build
    mkdir -p ~/.cursor/plugins/local
    ln -sfn '{{justfile_directory()}}' ~/.cursor/plugins/local/brave-history

# Format, lint, and typecheck
check:
    biome check --write ./src ./scripts
    tsc --noEmit

# Run the CLI from source (type-stripped) with optional args
run *ARGS:
    node src/index.ts {{ARGS}}

# Run the CLI from source and restart on file changes
dev *ARGS:
    node --watch src/index.ts {{ARGS}}

# Install dependencies
setup:
    npm install

# Check, then run the tests
test: check
    node --test "src/**/*.test.ts"

# Bump version, test, build, commit, tag, push, publish
release *ARGS:
    node scripts/release.ts {{ARGS}}
