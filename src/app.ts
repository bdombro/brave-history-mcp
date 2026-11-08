/*
Reference AppSpec — builtins on; command registration only.
The version is read from `package.json` at runtime, so the same code runs from
`src/` (`node src/index.ts`) and from the committed bundle (`dist/brave-history.mjs`).
*/

import { readFileSync } from "node:fs";
import { argsbarg } from "argsbarg";
import { echoCommand } from "./commands/echo/command.ts";
import { renderJsonCommand } from "./commands/render-json/command.ts";
import { statusCommand } from "./commands/status/command.ts";
import { workspacesCommand } from "./commands/workspaces/command.ts";
import { createIdentity } from "./create-identity.ts";
import { AppDb } from "./db/index.ts";

/** Reads a file from the plugin root (one level above `src/` or `dist/`). */
function packageFile(
  /** Path relative to the plugin root. */
  rel: string,
): string {
  return readFileSync(new URL(`../${rel}`, import.meta.url), "utf8");
}

/** Plugin version from `package.json` (bumped with the manifests by `npm run release`). */
const version = (JSON.parse(packageFile("package.json")) as { version: string }).version;

export const app = argsbarg({
  commands: [echoCommand, renderJsonCommand, statusCommand, workspacesCommand],
  description: createIdentity.desc,
  hooks: {
    beforeInvoke: AppDb.attach,
  },
  httpServer: { enabled: true },
  key: createIdentity.key,
  mcpServer: { enabled: true },
  readiness: AppDb.checkReadiness,
  version,
});
