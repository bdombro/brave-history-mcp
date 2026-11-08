/* App-level schema checks: every input and config object rejects unknown keys (z.strictObject). */

import assert from "node:assert/strict";
import { describe, test } from "node:test";
import { schemaStrictnessWarnings } from "argsbarg";
import { app } from "./app.ts";

describe("mcpPlugin schemas", () => {
  test("input and config schemas are strict", () => {
    assert.deepEqual(schemaStrictnessWarnings(app.spec), []);
  });
});
