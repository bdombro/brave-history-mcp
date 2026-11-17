# brave-history-mcp: Contributing

User installation and configuration are in [README.md](../README.md).

<!-- cpp-templates:begin -->
## Contents

- [Develop and test](#develop-and-test)
- [Unreleased libraries](#unreleased-libraries)
- [Rules](#rules)
- [PR, CI, and release flow](#pr-ci-and-release-flow)
- [Maintainer setup](#maintainer-setup)
- [Dependency bumps](#dependency-bumps)

## Develop and test

```bash
just doctor
just test
just lint
```

Iterate on a single doctest case with `just test "<case>"` (Debug build with ASan + UBSan, `BUGSINK_DSN=""`). For `homebrew-mcp` repos, run `just test-mcp` as well.

## Unreleased libraries

vcpkg reads extra port dirs from `VCPKG_OVERLAY_PORTS`, and an overlay wins over the registry. Point it at a dir of symlinks, one per unreleased lib; manifests never change:

```sh
mkdir -p ~/.vcpkg/overlays/dev
ln -s ~/dev/slimr/<lib>/ports/slimr-<lib> ~/.vcpkg/overlays/dev/slimr-<lib>
export VCPKG_OVERLAY_PORTS=~/.vcpkg/overlays/dev
just test
```

Remove the symlink (or `unset VCPKG_OVERLAY_PORTS`) to build against the released lib again. Keep the export out of shell startup files.

## Rules

- Never push directly to `main`; all changes land through squash-merged pull requests.
- PRs touch no release files (`VERSION`, `CHANGELOG.md`, `versions/`, `Formula/*.rb`, `Casks/*.rb`).
- Write the PR title (plus optional body) as the changelog entry; squash-merge turns it into one commit on `main`.
- Sanitize everything (no secrets, tokens, DSNs, or personal data in commits, logs, or docs).
- Run `just test` (ASan + UBSan) and `just lint` before opening or updating a PR.

## PR, CI, and release flow

- Every PR runs `test.yml`: a `gate` job skips macOS/Linux builds on docs-only PRs, and a single `ci` summary job gates merge.
- Merging to `main` triggers `release.yml` (one release at a time). It collects squash commits since the last tag (skipping docs-only ranges), picks the bump from merged PR labels (`patch` default, or `release:minor` / `release:major`; agents must ask before adding labels), writes `VERSION`/`vcpkg.json`, `CHANGELOG.md` (and `vcpkg x-add-version` for libraries), commits as the GitHub App, tags `vX.Y.Z`, and publishes the GitHub Release (plus assets and `Formula/*.rb` / `Casks/*.rb` updates).
- Pre-releases are a known option (tag/channel), not currently built.

## Maintainer setup

Direct pushes to `main` are blocked; `release.yml` uses a dedicated GitHub App so it can commit release metadata and formula updates without giving humans or agents an admin bypass.

### 1. Create and install the release GitHub App

Create one GitHub App (Settings -> Developer settings -> GitHub Apps):

- **Homepage URL:** any URL (for example, the owner's GitHub profile)
- **Callback / Setup URLs:** blank; **Webhook Active:** unchecked
- **Repository permissions:** `Contents: Read and write`, `Metadata: Read-only`
- **Where can this GitHub App be installed?:** Any account

Record `<app-id>`, generate a private key, and save it locally if needed for manual recovery (it can always be regenerated):

```sh
chmod 600 ~/.ssh/<app-name>.private-key.pem
```

Install the App on each owner (org or user) and select the target repositories.

### 2. Store Actions secrets

Org-level (requires org owner and `admin:org` scope):

```sh
gh auth refresh -h github.com -s admin:org
gh secret set RELEASE_APP_ID --org <owner> --visibility all --body "<app-id>"
gh secret set RELEASE_APP_PRIVATE_KEY --org <owner> --visibility all < ~/.ssh/<app-name>.private-key.pem
gh secret list --org <owner>
```

Per-repo (for user-owned repos or selected repos):

```sh
gh secret set RELEASE_APP_ID -R <owner>/<repo> --body "<app-id>"
gh secret set RELEASE_APP_PRIVATE_KEY -R <owner>/<repo> < ~/.ssh/<app-name>.private-key.pem
gh secret list -R <owner>/<repo>
```

`gh secret list` prints secret names only.

### 3. Protect `main`

Create (or `PUT /repos/<owner>/<repo>/rulesets/<ruleset-id>` to update) the `main` ruleset with App-only bypass, required `ci` check, squash-only merges, and no force push or deletion, then enforce squash-only merges on the repo:

```sh
gh api -X POST /repos/<owner>/<repo>/rulesets --input - <<'JSON'
{
  "name": "main",
  "target": "branch",
  "enforcement": "active",
  "conditions": {
    "ref_name": { "include": ["~DEFAULT_BRANCH"], "exclude": [] }
  },
  "bypass_actors": [
    { "actor_id": <app-id>, "actor_type": "Integration", "bypass_mode": "always" }
  ],
  "rules": [
    { "type": "deletion" },
    { "type": "non_fast_forward" },
    {
      "type": "pull_request",
      "parameters": {
        "required_approving_review_count": 0,
        "dismiss_stale_reviews_on_push": false,
        "require_code_owner_review": false,
        "require_last_push_approval": false,
        "required_review_thread_resolution": false,
        "allowed_merge_methods": ["squash"]
      }
    },
    {
      "type": "required_status_checks",
      "parameters": {
        "strict_required_status_checks_policy": false,
        "required_status_checks": [{ "context": "ci" }]
      }
    }
  ]
}
JSON

gh api -X PATCH /repos/<owner>/<repo> \
  -F allow_squash_merge=true \
  -F allow_merge_commit=false \
  -F allow_rebase_merge=false

gh api /repos/<owner>/<repo>/rulesets
```

### 4. Act as the App (emergency fix or manual release step)

Mint a short-lived JWT (`RS256`, `iat` = now - 60s, `exp` = now + 540s, `iss` = `<app-id>`), exchange it for a 1-hour installation token, and keep all credentials in shell variables only (never echo, log, or commit them):

```sh
app_id=<app-id>
pem=~/.ssh/<app-name>.private-key.pem
b64url() { openssl base64 -e -A | tr '+/' '-_' | tr -d '='; }
now=$(date +%s)
header=$(printf '{"alg":"RS256","typ":"JWT"}' | b64url)
payload=$(printf '{"iat":%d,"exp":%d,"iss":"%s"}' $((now - 60)) $((now + 540)) "$app_id" | b64url)
sig=$(printf '%s.%s' "$header" "$payload" | openssl dgst -sha256 -sign "$pem" | b64url)
jwt="${header}.${payload}.${sig}"

inst_id=$(curl -fsSL -H "Authorization: Bearer $jwt" -H "Accept: application/vnd.github+json" \
  "https://api.github.com/repos/<owner>/<repo>/installation" | jq -r .id)
token=$(curl -fsSL -X POST -H "Authorization: Bearer $jwt" -H "Accept: application/vnd.github+json" \
  "https://api.github.com/app/installations/${inst_id}/access_tokens" | jq -r .token)
```

Use `$token` with `gh` or `git push`, committing as the App's bot identity (`gh api /users/<app-slug>%5Bbot%5D --jq .id` gives `<bot-user-id>`):

```sh
git -c user.name="<app-slug>[bot]" \
    -c user.email="<bot-user-id>+<app-slug>[bot]@users.noreply.github.com" \
    commit -m "fix: ..."
git push "https://x-access-token:${token}@github.com/<owner>/<repo>.git" HEAD:main
GH_TOKEN="$token" gh release create ...
```

Tokens expire after 1 hour; revoke early when done:

```sh
curl -fsSL -X DELETE -H "Authorization: Bearer $token" -H "Accept: application/vnd.github+json" \
  https://api.github.com/installation/token
unset jwt token
```

## Dependency bumps

Bump a library baseline in `vcpkg-configuration.json` only when a bug fix, new API, or security fix requires it; otherwise wait for the next `just upgrade-deps`.
<!-- cpp-templates:end -->

## Repository tips

Run the direct binary on a separate dev port:

```bash
./build/bin/brave-history-mcp http --port 20003
```

### Platforms

`brave-history-mcp` targets macOS (`platforms := "macos"`) because it reads desktop Brave Browser profiles. On macOS, builds use Homebrew `llvm@23` (`clang`/`clang++`) with static `libc++` and symbol hiding targeting macOS 15.0+, with no runtime dependency on Homebrew LLVM.

### Transports & MCP `2026-07-28`

- **Streamable HTTP**: `brave-history-mcp http` (default `127.0.0.1:10003`, override with `--host` / `--port`; pass `--host 0.0.0.0` to expose)
- **Direct CLI**: `brave-history-mcp <tool> ['{"arg":"val"}']` (defaults to `{}`)

This server targets **MCP protocol version `2026-07-28`** (`server/discover` instead of legacy `initialize`).
