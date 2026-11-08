/*
Echo leaf — minimal MCP-friendly command; `ctx.inputs` is typed from the option literal (command).
*/

import { command, OptionKind } from "argsbarg";

/** Prints (CLI) or returns (MCP/HTTP) the `message` option. */
export const echoCommand = command({
  key: "echo",
  description: "Echo a message (MCP-friendly leaf).",
  options: [
    {
      name: "message",
      description: "Text to print.",
      kind: OptionKind.String,
      required: true,
    },
  ],
  handler: (ctx) => {
    const { message } = ctx.inputs;
    if (ctx.invocation === "cli") {
      console.log(message);
      return;
    }
    return message;
  },
});
