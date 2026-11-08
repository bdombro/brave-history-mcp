/*
Plugin release script (`just release <major|minor|patch>`).
Runs tests, bumps the version in `package.json` and both plugin manifests, promotes the changelog, rebuilds the committed bundle, commits all changes, tags, pushes, and creates a GitHub release.
*/

import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, writeFileSync } from "node:fs";

/** Allowed semver bump kinds. */
type Bump = "major" | "minor" | "patch";

/** Files whose `"version"` field is bumped together. */
const VERSIONED_FILES = ["package.json", ".cursor-plugin/plugin.json", ".claude-plugin/plugin.json"];

/** Runs a command with inherited stdio; throws (ending the script) when it fails. */
function run(
  /** Executable name. */
  cmd: string,
  /** Arguments. */
  args: string[],
): void {
  execFileSync(cmd, args, { stdio: "inherit" });
}

/** Applies a semver bump to `current`. */
function applyBump(current: string, bump: Bump): string {
  const [major, minor, patch] = current.split(".").map(Number) as [number, number, number];
  if (bump === "major") return `${major + 1}.0.0`;
  if (bump === "minor") return `${major}.${minor + 1}.0`;
  return `${major}.${minor}.${patch + 1}`;
}

const argv = process.argv.slice(2);
const bump = argv.find((a): a is Bump => a === "major" || a === "minor" || a === "patch");
if (!bump || argv.length !== 1) {
  process.stderr.write("Usage: just release <major|minor|patch>\n");
  process.exit(1);
}
const current = (JSON.parse(readFileSync("package.json", "utf8")) as { version: string }).version;
const next = applyBump(current, bump);

run("just", ["test"]);
for (const path of VERSIONED_FILES) {
  if (!existsSync(path)) continue;
  writeFileSync(path, readFileSync(path, "utf8").replace(/"version":\s*"[^"]+"/, `"version": "${next}"`));
}
const changelog = readFileSync("CHANGELOG.md", "utf8");
const date = new Date().toISOString().slice(0, 10);
writeFileSync("CHANGELOG.md", changelog.replace(/^## \[Unreleased\]/m, `## [Unreleased]\n\n## [${next}] - ${date}`));
run("just", ["build"]);
run("git", ["add", "-A"]);
run("git", ["commit", "-m", `chore: release v${next}`]);
run("git", ["tag", `v${next}`]);
run("git", ["push"]);
run("git", ["push", "origin", `v${next}`]);
run("gh", ["release", "create", `v${next}`, "--title", `v${next}`, "--generate-notes"]);
console.log(`Released v${next}`);
