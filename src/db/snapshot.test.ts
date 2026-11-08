import assert from "node:assert/strict";
import { chmodSync, existsSync, readdirSync, statSync, utimesSync } from "node:fs";
import { dirname, join } from "node:path";
import { DatabaseSync } from "node:sqlite";
import { before, describe, test } from "node:test";
import { addPage, createFixture, type Fixture, historyPath } from "./fixture.ts";
import { queryPages } from "./index.ts";
import { ensureCachedHistory, listProfiles, selectProfiles } from "./snapshot.ts";

const page = (url: string, at: string) => ({ url, title: url, visits: [{ at }] });
const urls = () => queryPages({ words: [], limit: 50 }).map((p) => p.url);

describe("cached History copy", () => {
  let fixture: Fixture;
  let source: string;

  before(() => {
    fixture = createFixture(
      { Default: [page("https://a.example/", "2026-01-01T10:00:00Z")], "Profile 2": [], Junk: [] },
      { Default: "Personal", "Profile 2": "Work" },
    );
    source = historyPath(fixture, "Default");
    setMtime(source, Date.parse("2026-01-01T10:00:00Z"));
  });

  test("discovers Default and Profile N, with display names", () => {
    assert.deepEqual(listProfiles(), [
      { dir: "Default", name: "Personal" },
      { dir: "Profile 2", name: "Work" },
    ]);
    assert.deepEqual(
      selectProfiles("work").map((p) => p.dir),
      ["Profile 2"],
    );
    assert.throws(() => selectProfiles("nope"), /No Brave profile matches "nope"/);
  });

  test("copies on first use and stamps the copy with the original's mtime", () => {
    const cached = ensureCachedHistory({ dir: "Default" });
    assert.equal(cached, join(fixture.cache, "Default", "History"));
    assert.equal(statSync(cached).mtimeMs, statSync(source).mtimeMs);
    assert.deepEqual(urls(), ["https://a.example/"]);
  });

  test("skips the copy while the original is not newer", () => {
    addToSource(page("https://b.example/", "2026-01-02T10:00:00Z"));
    setMtime(source, Date.parse("2026-01-01T10:00:00Z"));
    assert.deepEqual(urls(), ["https://a.example/"]);
  });

  test("re-copies when the original is newer", () => {
    setMtime(source, Date.parse("2026-01-03T10:00:00Z"));
    assert.deepEqual(urls(), ["https://b.example/", "https://a.example/"]);
  });

  test("re-copies when only the -journal sidecar is newer", () => {
    addToSource(page("https://c.example/", "2026-01-04T10:00:00Z"));
    setMtime(source, Date.parse("2026-01-03T10:00:00Z"));
    const journal = `${source}-journal`;
    const db = new DatabaseSync(":memory:");
    db.exec(`VACUUM INTO '${journal}'`);
    db.close();
    setMtime(journal, Date.parse("2026-01-05T10:00:00Z"));
    assert.deepEqual(urls(), ["https://c.example/", "https://b.example/", "https://a.example/"]);
    assert.equal(statSync(join(fixture.cache, "Default", "History")).mtimeMs, Date.parse("2026-01-05T10:00:00Z"));
  });

  test("never writes to the Brave data dir", () => {
    const before = readdirSync(join(fixture.root, "Default")).sort();
    const mtime = statSync(source).mtimeMs;
    setMtime(source, mtime + 60_000);
    urls();
    assert.deepEqual(readdirSync(join(fixture.root, "Default")).sort(), before);
    assert.equal(statSync(source).mtimeMs, mtime + 60_000);
    assert.equal(existsSync(`${source}-wal`), false);
  });

  test("refuses a cache dir inside the Brave data dir", () => {
    const saved = process.env.BRAVE_HISTORY_CACHE;
    process.env.BRAVE_HISTORY_CACHE = join(fixture.root, "cache");
    try {
      assert.throws(() => ensureCachedHistory({ dir: "Default" }), /Refusing to cache/);
    } finally {
      process.env.BRAVE_HISTORY_CACHE = saved;
    }
  });

  test("cache dir is 0o700 and the copied files 0o600, including pre-existing loose ones", () => {
    const dir = join(fixture.cache, "Default");
    const mode = (p: string) => statSync(p).mode & 0o777;
    setMtime(source, Date.parse("2026-01-06T10:00:00Z"));
    ensureCachedHistory({ dir: "Default" });
    assert.equal(mode(fixture.cache), 0o700);
    assert.equal(mode(dir), 0o700);
    assert.equal(mode(join(dir, "History")), 0o600);
    chmodSync(dir, 0o755);
    chmodSync(join(dir, "History"), 0o644);
    ensureCachedHistory({ dir: "Default" }); // up to date: no copy, still tightened
    assert.equal(mode(dir), 0o700);
    assert.equal(mode(join(dir, "History")), 0o600);
  });

  test("staging uses mkdtemp and leaves no temp dirs behind", () => {
    setMtime(source, Date.parse("2026-01-07T10:00:00Z"));
    ensureCachedHistory({ dir: "Default" });
    assert.deepEqual(
      readdirSync(fixture.cache).filter((n) => n.includes(".tmp-")),
      [],
    );
  });

  function addToSource(p: ReturnType<typeof page>): void {
    const db = new DatabaseSync(source);
    addPage(db, p);
    db.close();
  }
});

function setMtime(path: string, ms: number): void {
  utimesSync(path, new Date(ms), new Date(ms));
}
