/*
Status leaf — demonstrates outputSchema.
*/

import { command, OptionKind } from "argsbarg";
import { z } from "zod";

/** JSON stdout for `brave-history status --json`. */
export const StatusJsonOutput = z.strictObject({
  version: z.string().describe("App version from program root."),
});

/** `status --json` payload. */
export type StatusJsonOutput = z.infer<typeof StatusJsonOutput>;

/** Prints the app version (JSON with `--json`). */
export const statusCommand = command({
  key: "status",
  description: "Show app version.",
  options: [
    {
      name: "json",
      description: "Emit JSON.",
      kind: OptionKind.Presence,
    },
  ],
  outputSchema: StatusJsonOutput,
  handler: (ctx) => {
    const out: StatusJsonOutput = { version: ctx.spec.version };
    if (ctx.invocation === "cli" && !ctx.hasFlag("json")) {
      console.log(`version=${out.version}`);
      return;
    }
    // Returned, not printed, so MCP and --json get a structured result.
    return out;
  },
});
