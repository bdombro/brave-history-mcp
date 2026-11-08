import assert from "node:assert/strict";
import { beforeEach, describe, test } from "node:test";
import { type AppSpec, argsbarg } from "argsbarg";
import { AppDb } from "../../db/index.ts";
import { workspacesTestSpec } from "./command.ts";

const baseSpec: AppSpec = {
  key: "brave-history",
  version: "1.0.0",
  description: "Demo.",
  httpServer: { enabled: true },
  commands: [],
  hooks: {
    beforeInvoke: AppDb.attach,
  },
};

describe("workspaces command", () => {
  const spec = workspacesTestSpec(baseSpec);
  const cli = argsbarg(spec);

  beforeEach(() => {
    AppDb.resetForTests();
  });

  test("GET workspaces lists empty collection", async () => {
    const result = await cli.invoke(["workspaces", "get"], { invocation: "http" });
    assert.equal(result.kind, "ok");
    assert.deepEqual(result.response?.body, { workspaces: [] });
  });

  test("POST workspaces creates resource", async () => {
    const created = await cli.invoke(["workspaces", "post"], {
      invocation: "http",
      toolArgs: { name: "qa2" },
    });
    assert.equal(created.kind, "ok");
    const body = created.response?.body as { id: string; name: string };
    assert.equal(body.name, "qa2");
    assert.ok(body.id.length > 0);

    const got = await cli.invoke(["workspaces", body.id, "get"], { invocation: "http" });
    assert.equal(got.kind, "ok");
    assert.deepEqual(got.response?.body, body);
  });

  test("CLI workspaces :id get resolves path param", async () => {
    const created = await cli.invoke(["workspaces", "post"], {
      invocation: "http",
      toolArgs: { name: "cli-ws" },
    });
    assert.equal(created.kind, "ok");
    const id = (created.response?.body as { id: string } | undefined)?.id ?? "";

    const got = await cli.invoke(["workspaces", id, "get"], { invocation: "http" });
    assert.equal(got.kind, "ok");
    assert.deepEqual(got.response?.body, { id, name: "cli-ws" });
  });
});
