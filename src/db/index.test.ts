import assert from "node:assert/strict";
import { before, describe, test } from "node:test";
import { createFixture } from "./fixture.ts";
import { queryPages, queryVisits } from "./index.ts";

const SERVER_REDIRECT = 0x80000000;
const CHAIN_START = 0x10000000;

before(() => {
  createFixture({
    Default: [
      {
        url: "https://www.amazon.com/Standing-Desk/dp/B01",
        title: "FlexiSpot Standing Desk",
        visits: [
          { at: "2026-06-30T22:30:00Z", transition: 1 | CHAIN_START }, // 23:30 Dublin, 30 Jun
          { at: "2026-07-02T10:00:00Z", transition: 0, durationMicros: 95_000_000 },
        ],
      },
      { url: "https://amazon.com/", title: "Amazon", visits: [{ at: "2026-07-01T09:00:00Z" }] },
      { url: "https://notamazon.com/desk", title: "Desk deals", visits: [{ at: "2026-07-01T09:30:00Z" }] },
      { url: "https://smile.amazon.com:8443/x", title: "Smile", visits: [{ at: "2025-12-31T23:59:00Z" }] },
      {
        url: "https://example.com/100%25-off",
        title: "100% off sale",
        visits: [{ at: "2026-01-02T00:00:00Z", transition: 8 | SERVER_REDIRECT }],
      },
      {
        url: "https://example.com/hidden",
        title: "Hidden desk frame",
        hidden: true,
        visits: [{ at: "2026-07-03T00:00:00Z" }],
      },
    ],
    "Profile 1": [
      {
        url: "https://news.example.org/standing-desks",
        title: "Best standing desks",
        visits: [{ at: "2026-07-01T12:00:00Z" }],
      },
    ],
  });
});

describe("queryPages (search/list)", () => {
  const search = async (words: string[], extra: Partial<Parameters<typeof queryPages>[0]> = {}) =>
    (await queryPages({ words, limit: 25, ...extra })).map((p) => p.url);

  test("ANDs words across title and URL, case-insensitive, across profiles newest first", async () => {
    assert.deepEqual(await search(["STANDING", "desk"]), [
      "https://www.amazon.com/Standing-Desk/dp/B01",
      "https://news.example.org/standing-desks",
    ]);
    assert.deepEqual(await search(["flexispot", "amazon"]), ["https://www.amazon.com/Standing-Desk/dp/B01"]);
    assert.deepEqual(await search(["flexispot", "ebay"]), []);
  });

  test("treats % and _ literally", async () => {
    assert.deepEqual(await search(["100%"]), ["https://example.com/100%25-off"]);
    assert.deepEqual(await search(["_"]), []);
  });

  test("excludes hidden URLs", async () => {
    assert.deepEqual(await search(["frame"]), []);
  });

  test("domain matches the host and subdomains (with ports), not lookalikes", async () => {
    assert.deepEqual(await search([], { domain: "amazon.com" }), [
      "https://www.amazon.com/Standing-Desk/dp/B01",
      "https://amazon.com/",
      "https://smile.amazon.com:8443/x",
    ]);
    assert.deepEqual(await search([], { domain: "https://WWW.Amazon.com/whatever" }), [
      "https://www.amazon.com/Standing-Desk/dp/B01",
    ]);
  });

  test("date range is inclusive local days and reports the last visit inside it", async () => {
    const pages = await queryPages({ words: ["desk"], from: "2026-06-30", to: "2026-06-30", limit: 25 });
    assert.deepEqual(
      pages.map((p) => [p.url, p.lastVisit, p.visitCount]),
      [["https://www.amazon.com/Standing-Desk/dp/B01", "2026-06-30T23:30:00+01:00", 2]],
    );
    assert.deepEqual(await search([], { from: "2026-07-01", to: "2026-07-01" }), [
      "https://news.example.org/standing-desks",
      "https://notamazon.com/desk",
      "https://amazon.com/",
    ]);
    assert.deepEqual(await search([], { to: "2025-12-31" }), ["https://smile.amazon.com:8443/x"]);
    assert.deepEqual(await search(["desk"], { from: "2026-07-02" }), ["https://www.amazon.com/Standing-Desk/dp/B01"]);
  });

  test("profile filter and limit", async () => {
    assert.deepEqual(await search(["desk"], { profile: "Profile 1" }), ["https://news.example.org/standing-desks"]);
    assert.equal((await search([], { limit: 2 })).length, 2);
  });

  test("returns title, ISO Dublin last visit, visit count and profile", async () => {
    const [row] = await queryPages({ words: ["flexispot"], limit: 1 });
    assert.deepEqual(row, {
      title: "FlexiSpot Standing Desk",
      url: "https://www.amazon.com/Standing-Desk/dp/B01",
      lastVisit: "2026-07-02T11:00:00+01:00",
      visitCount: 2,
      profile: "Default",
    });
  });
});

describe("queryVisits", () => {
  test("exact URL returns every visit with transition and duration, newest first", async () => {
    assert.deepEqual(
      await queryVisits({ url: "https://www.amazon.com/Standing-Desk/dp/B01", prefix: false, limit: 25 }),
      [
        {
          time: "2026-07-02T11:00:00+01:00",
          url: "https://www.amazon.com/Standing-Desk/dp/B01",
          title: "FlexiSpot Standing Desk",
          transition: "link",
          durationSeconds: 95,
          profile: "Default",
        },
        {
          time: "2026-06-30T23:30:00+01:00",
          url: "https://www.amazon.com/Standing-Desk/dp/B01",
          title: "FlexiSpot Standing Desk",
          transition: "typed",
          durationSeconds: 0,
          profile: "Default",
        },
      ],
    );
  });

  test("exact URL also tries a trailing slash; qualifier bits are masked off", async () => {
    assert.deepEqual(
      (await queryVisits({ url: "https://amazon.com", prefix: false, limit: 25 })).map((v) => v.url),
      ["https://amazon.com/"],
    );
    assert.equal(
      (await queryVisits({ url: "https://example.com/100%25-off", prefix: false, limit: 25 }))[0].transition,
      "reload",
    );
  });

  test("prefix matches URLs under it and honours the date range", async () => {
    assert.deepEqual(
      (await queryVisits({ url: "https://www.amazon.com/", prefix: true, limit: 25 })).map((v) => v.time),
      ["2026-07-02T11:00:00+01:00", "2026-06-30T23:30:00+01:00"],
    );
    assert.equal(
      (await queryVisits({ url: "https://www.amazon.com/", prefix: true, from: "2026-07-01", limit: 25 })).length,
      1,
    );
    assert.equal((await queryVisits({ url: "https://example.com/1", prefix: false, limit: 25 })).length, 0);
  });
});
