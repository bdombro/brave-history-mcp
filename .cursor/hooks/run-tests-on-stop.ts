#!/usr/bin/env node
/* Cursor stop hook: run `just test` on agent completion; follow up if it fails. */

import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { text } from "node:stream/consumers";

try {
  const empty = () => {
    console.log("{}");
    process.exit(0);
  };

  const input = JSON.parse(await text(process.stdin)) as {
    status?: string;
    loop_count?: number;
    workspace_roots?: string[];
  };

  if (input.status !== "completed") empty();

  const cwd = input.workspace_roots?.[0] ?? process.cwd();
  if (!existsSync(`${cwd}/justfile`)) empty();

  const diff = spawnSync("git", ["diff", "--name-only", "HEAD"], { cwd, encoding: "utf8" });
  const CODE_FILE = /\.(t|j)sx?$/i;
  const SKIP_PREFIX = /^(node_modules|dist|\.cursor)\//;
  const changed = (diff.stdout ?? "")
    .split("\n")
    .some((path) => path && (path === "justfile" || (CODE_FILE.test(path) && !SKIP_PREFIX.test(path))));
  if (!changed) empty();

  const proc = spawnSync("just", ["test"], { cwd, env: { ...process.env, FORCE_COLOR: "0" }, encoding: "utf8" });
  const output = (proc.stdout ?? "") + (proc.stderr ?? "");

  if (proc.status === 0) empty();

  const lines = output.trimEnd().split("\n");
  const tail = lines.length > 80 ? lines.slice(-80).join("\n") : output.trimEnd();
  const n = (input.loop_count ?? 0) + 1;

  console.log(
    JSON.stringify({
      followup_message: `Tests failed (auto-retry ${n}/20). Fix and ensure \`npm test\` passes.\n\n\`\`\`\n${tail}\n\`\`\``,
    }),
  );
} catch {
  console.log("{}");
}
