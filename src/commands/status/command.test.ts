import assert from "node:assert/strict";
import { describe, test } from "node:test";
import { statusCommand } from "./command.ts";

describe("status command", () => {
  test("exports outputSchema and json option", () => {
    assert.equal(statusCommand.key, "status");
    assert.notEqual(statusCommand.outputSchema, undefined);
    assert.equal(
      statusCommand.options?.some((o) => o.name === "json"),
      true,
    );
  });
});
