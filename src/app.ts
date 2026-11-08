/*
App spec — command registration only.
The version is read from `package.json` at runtime, so the same code runs from
`src/` (`node src/index.ts`) and from the committed bundle (`dist/brave-history.mjs`).
*/

import { readFileSync } from "node:fs";
import { argsbarg } from "argsbarg";
import { errors } from "./bugsink.ts";
import { listCommand } from "./commands/list/command.ts";
import { searchCommand } from "./commands/search/command.ts";
import { statusCommand } from "./commands/status/command.ts";
import { visitsCommand } from "./commands/visits/command.ts";
import { createIdentity } from "./create-identity.ts";

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
  errorReporter: errors,
  commands: [listCommand, searchCommand, statusCommand, visitsCommand],
  description: createIdentity.desc,
  key: createIdentity.key,
  mcpServer: {
    enabled: true,
    instructions:
      "Brian's Brave browser history (read-only). Use it to recall sites, products, articles, or docs he looked at: search by keywords, list recent pages by domain/date, or get every visit to a URL.",
  },
  version,
});
