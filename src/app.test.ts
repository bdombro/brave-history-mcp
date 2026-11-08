/* App-level checks: strict schemas and the CLI path end to end over a fixture History DB. */

import assert from "node:assert/strict";
import { describe, test } from "node:test";
import { schemaStrictnessWarnings } from "argsbarg";
import { app } from "./app.ts";
import { createFixture } from "./db/fixture.ts";

describe("app", () => {
  test("input and config schemas are strict", () => {
    assert.deepEqual(schemaStrictnessWarnings(app.spec), []);
  });

  test("exposes the history commands", () => {
    const commands = "commands" in app.spec ? (app.spec.commands as { key: string }[]) : [];
    assert.deepEqual(
      commands.map((c) => c.key),
      ["list", "search", "status", "visits"],
    );
  });

  test("search runs end to end on the MCP path", async () => {
    createFixture({
      Default: [{ url: "https://example.com/desk", title: "Standing desk", visits: [{ at: "2026-03-01T10:00:00Z" }] }],
    });
    const result = await app.invoke(["search", "standing", "desk"], { invocation: "mcp" });
    assert.equal(result.exitCode, 0, result.stderr);
    assert.deepEqual(result.response?.body, {
      pages: [
        {
          title: "Standing desk",
          url: "https://example.com/desk",
          lastVisit: "2026-03-01T10:00:00+00:00",
          visitCount: 1,
          profile: "Default",
        },
      ],
    });
  });

  describe("URL stripping", () => {
    const FULL = "https://example.com/desk?token=abc&id=1#frag";
    const fixtureWithQuery = () =>
      createFixture({ Default: [{ url: FULL, title: "Desk", visits: [{ at: "2026-03-01T10:00:00Z" }] }] });
    const urlsOf = (body: unknown, key: "pages" | "visits") =>
      (body as Record<string, { url: string }[]>)[key].map((r) => r.url);

    test("search, list and visits drop query and fragment by default", async () => {
      fixtureWithQuery();
      for (const [argv, key] of [
        [["search", "desk"], "pages"],
        [["list"], "pages"],
        [["visits", FULL], "visits"],
      ] as const) {
        const result = await app.invoke([...argv], { invocation: "mcp" });
        assert.equal(result.exitCode, 0, result.stderr);
        assert.deepEqual(urlsOf(result.response?.body, key), ["https://example.com/desk"], argv[0]);
      }
    });

    test("--fullUrls keeps the full URL", async () => {
      fixtureWithQuery();
      for (const [argv, key] of [
        [["search", "desk", "--fullUrls"], "pages"],
        [["list", "--fullUrls"], "pages"],
        [["visits", FULL, "--fullUrls"], "visits"],
      ] as const) {
        const result = await app.invoke([...argv], { invocation: "mcp" });
        assert.equal(result.exitCode, 0, result.stderr);
        assert.deepEqual(urlsOf(result.response?.body, key), [FULL], argv[0]);
      }
    });
  });
});
