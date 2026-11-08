/*
Render-json leaf — JSON body demo with a Zod inputSchema and typed ctx.inputs (command).
*/

import { type AppSpec, command } from "argsbarg";
import { z } from "zod";

/** JSON body for `render-json`. */
export const RenderJsonInput = z.strictObject({
  message: z.string().describe("Message to echo back."),
});

/** Parsed `render-json` body. */
export type RenderJsonInput = z.infer<typeof RenderJsonInput>;

/** Echoes the `message` field of a JSON body. */
export const renderJsonCommand = command({
  key: "render-json",
  description: "Echo a JSON message (schema-first JSON leaf demo).",
  kind: "document",
  inputSchema: RenderJsonInput,
  handler: (ctx) => {
    const { message } = ctx.inputs;
    if (ctx.invocation === "cli") {
      console.log(message);
      return;
    }
    return { message };
  },
});

/** App spec stub for colocated tests. */
export function renderJsonTestSpec(base: AppSpec): AppSpec {
  return {
    ...base,
    commands: [renderJsonCommand],
  };
}
