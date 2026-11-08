import assert from "node:assert/strict";
import { describe, test } from "node:test";
import { type AppSpec, argsbarg, command } from "argsbarg";
import { renderJsonCommand, renderJsonTestSpec } from "./command.ts";

const baseSpec: AppSpec = {
  key: "brave-history",
  version: "1.0.0",
  description: "Demo.",
  httpServer: { enabled: true },
  commands: [],
};

describe("render-json command", () => {
  const spec = renderJsonTestSpec(baseSpec);
  const cli = argsbarg(spec);

  test("HTTP invoke returns echoed message", async () => {
    const result = await cli.invoke(["render-json"], {
      invocation: "http",
      toolArgs: { message: "hello" },
    });
    assert.equal(result.kind, "ok");
    assert.deepEqual(result.response?.body, { message: "hello" });
  });

  test("rejects invalid input before handler via inputSchema", async () => {
    let handlerCalled = false;
    const badProgram = renderJsonTestSpec({
      ...baseSpec,
      commands: [
        command({
          ...renderJsonCommand,
          handler: () => {
            handlerCalled = true;
          },
        }),
      ],
    });
    const result = await argsbarg(badProgram).invoke(["render-json"], {
      invocation: "http",
      toolArgs: { message: 123 },
    });
    assert.equal(result.kind, "error");
    assert.equal(handlerCalled, false);
  });
});
