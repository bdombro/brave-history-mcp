set shell := ["zsh", "-f", "-eu", "-o", "pipefail", "-c"]
set positional-arguments

architecture := replace(replace(arch(), "aarch64", "arm64"), "x86_64", "x64")
system := if os() == "macos" { "osx" } else { os() }
triplet := f'{{architecture}}-{{system}}'
compiler := if os() == "macos" { "/usr/bin/clang++" } else { "clang++" }

_:
    @just --list

deps:
    vcpkg install --x-wait-for-lock --triplet={{triplet}} --x-install-root=build/deps

configure configuration="Debug": deps
    CXX="${CXX:-{{compiler}}}" cmake -S . -B build/cmake/{{configuration}} -G Ninja \
        -DCMAKE_BUILD_TYPE={{configuration}} \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DVCPKG_MANIFEST_MODE=OFF \
        -DVCPKG_INSTALLED_DIR="{{justfile_directory()}}/build/deps" \
        -DVCPKG_TARGET_TRIPLET={{triplet}} \
        -DENABLE_SANITIZERS={{if configuration == "Debug" { "ON" } else { "OFF" }}}

test: (configure "Debug")
    cmake --build build/cmake/Debug --parallel 2
    ctest --test-dir build/cmake/Debug --output-on-failure --no-tests=error

build: (configure "Release")
    cmake --build build/cmake/Release --parallel 2
    ctest --test-dir build/cmake/Release --output-on-failure --no-tests=error

lint: (configure "Debug")
    "${CLANG_TIDY:-clang-tidy}" -p build/cmake/Debug src/main.cpp tests/test_main.cpp

test-mcp: build
    node scripts/test-mcp.mjs build/bin/brave-history-mcp search,list,visits,status

install: install-local

install-local: build
    @brew_prefix="$(brew --prefix)" && \
    tap_parent="$brew_prefix/Library/Taps/bdombro" && \
    tap_path="$tap_parent/homebrew-brave-history-mcp" && \
    mkdir -p "$tap_parent" && \
    ln -sfn "{{justfile_directory()}}" "$tap_path" && \
    trap './scripts/dev-formula.sh reset' EXIT && \
    ./scripts/dev-formula.sh install && \
    if brew list --formula bdombro/brave-history-mcp/brave-history-mcp >/dev/null 2>&1; then \
        HOMEBREW_NO_ASK=1 brew reinstall --formula bdombro/brave-history-mcp/brave-history-mcp; \
    else \
        HOMEBREW_NO_ASK=1 brew install --formula bdombro/brave-history-mcp/brave-history-mcp; \
    fi

reinstall-local: build
    @was_running=0; \
    if brew services list | grep -E '^brave-history-mcp[[:space:]]+started' >/dev/null; then \
        was_running=1; \
        brew services stop bdombro/brave-history-mcp/brave-history-mcp; \
    fi; \
    just install-local; \
    if [ "$was_running" -eq 1 ]; then \
        brew services start bdombro/brave-history-mcp/brave-history-mcp; \
        just wait-ready; \
    fi

start:
    brew services start bdombro/brave-history-mcp/brave-history-mcp
    just wait-ready

stop:
    brew services stop bdombro/brave-history-mcp/brave-history-mcp

restart:
    brew services restart bdombro/brave-history-mcp/brave-history-mcp
    just wait-ready

wait-ready:
    @for i in $(seq 1 30); do \
        if curl -sS -o /dev/null -w "%{http_code}" -X POST http://127.0.0.1:8083/mcp \
            -H 'Content-Type: application/json' \
            -H 'Accept: application/json, text/event-stream' \
            -H 'MCP-Protocol-Version: 2026-07-28' \
            -H 'Mcp-Method: server/discover' \
            -d '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}' | grep -x '200' >/dev/null; then \
            echo "endpoint http://127.0.0.1:8083/mcp: ready"; \
            exit 0; \
        fi; \
        sleep 0.5; \
    done; \
    echo "endpoint http://127.0.0.1:8083/mcp: did not become ready" >&2; \
    exit 1

status:
    brew services info bdombro/brave-history-mcp/brave-history-mcp
    @if curl -sS -o /dev/null -w "%{http_code}" -X POST http://127.0.0.1:8083/mcp \
        -H 'Content-Type: application/json' \
        -H 'Accept: application/json, text/event-stream' \
        -H 'MCP-Protocol-Version: 2026-07-28' \
        -H 'Mcp-Method: server/discover' \
        -d '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}' | grep -x '200' >/dev/null; then \
        echo "endpoint http://127.0.0.1:8083/mcp: ready"; \
    else \
        echo "endpoint http://127.0.0.1:8083/mcp: not reachable"; \
    fi

uninstall:
    @if brew services list | grep -E '^brave-history-mcp[[:space:]]+started' >/dev/null; then brew services stop bdombro/brave-history-mcp/brave-history-mcp; fi
    @if brew list --formula bdombro/brave-history-mcp/brave-history-mcp >/dev/null 2>&1; then HOMEBREW_NO_ASK=1 brew uninstall --formula bdombro/brave-history-mcp/brave-history-mcp; fi
    @if brew tap | grep -x 'bdombro/brave-history-mcp' >/dev/null; then HOMEBREW_NO_ASK=1 brew untap bdombro/brave-history-mcp; fi

run *args:
    ./build/bin/brave-history-mcp "$@"

clean:
    rm -rf build
