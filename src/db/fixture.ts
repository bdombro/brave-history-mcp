/*
Test-only fixture: a temp Brave data dir with Chromium-shaped History DBs, wired via env overrides.
*/

import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { DatabaseSync } from "node:sqlite";
import { unixMsToChromeMicros } from "./time.ts";

/** One `urls` row plus its `visits`. */
export interface FixturePage {
  hidden?: boolean;
  title: string;
  url: string;
  /** Visits as ISO instants, optionally with a raw Chromium transition value. */
  visits: { at: string; durationMicros?: number; transition?: number }[];
}

/** Paths of a created fixture. */
export interface Fixture {
  cache: string;
  root: string;
}

/** Path of a fixture profile's History DB. */
export function historyPath(fixture: Fixture, profileDir: string): string {
  return join(fixture.root, profileDir, "History");
}

/**
 * Creates a temp Brave root (profiles → pages) and cache dir, and points
 * `BRAVE_HISTORY_ROOT` / `BRAVE_HISTORY_CACHE` / `BRAVE_HISTORY_TZ` at them.
 */
export function createFixture(profiles: Record<string, FixturePage[]>, names: Record<string, string> = {}): Fixture {
  const base = mkdtempSync(join(tmpdir(), "brave-history-test-"));
  const fixture = { root: join(base, "Brave-Browser"), cache: join(base, "cache") };
  mkdirSync(fixture.root, { recursive: true });
  const infoCache = Object.fromEntries(Object.entries(names).map(([dir, name]) => [dir, { name }]));
  writeFileSync(join(fixture.root, "Local State"), JSON.stringify({ profile: { info_cache: infoCache } }));
  for (const [dir, pages] of Object.entries(profiles)) {
    mkdirSync(join(fixture.root, dir), { recursive: true });
    const db = new DatabaseSync(historyPath(fixture, dir));
    db.exec(`
      CREATE TABLE urls (id INTEGER PRIMARY KEY AUTOINCREMENT, url LONGVARCHAR, title LONGVARCHAR,
        visit_count INTEGER DEFAULT 0 NOT NULL, typed_count INTEGER DEFAULT 0 NOT NULL,
        last_visit_time INTEGER NOT NULL, hidden INTEGER DEFAULT 0 NOT NULL);
      CREATE TABLE visits (id INTEGER PRIMARY KEY, url INTEGER NOT NULL, visit_time INTEGER NOT NULL,
        from_visit INTEGER, transition INTEGER DEFAULT 0 NOT NULL, segment_id INTEGER,
        visit_duration INTEGER DEFAULT 0 NOT NULL);`);
    for (const page of pages) {
      addPage(db, page);
    }
    db.close();
  }
  process.env.BRAVE_HISTORY_ROOT = fixture.root;
  process.env.BRAVE_HISTORY_CACHE = fixture.cache;
  process.env.BRAVE_HISTORY_TZ = "Europe/Dublin";
  return fixture;
}

/** Inserts a page and its visits into an open History DB. */
export function addPage(db: DatabaseSync, page: FixturePage): void {
  const times = page.visits.map((v) => unixMsToChromeMicros(Date.parse(v.at)));
  const last = [...times].sort((a, b) => (a < b ? 1 : -1))[0] ?? 0n;
  const { lastInsertRowid } = db
    .prepare("INSERT INTO urls (url, title, visit_count, last_visit_time, hidden) VALUES (?, ?, ?, ?, ?)")
    .run(page.url, page.title, page.visits.length, last, page.hidden ? 1 : 0);
  page.visits.forEach((visit, i) => {
    db.prepare("INSERT INTO visits (url, visit_time, transition, visit_duration) VALUES (?, ?, ?, ?)").run(
      lastInsertRowid,
      times[i],
      visit.transition ?? 0,
      visit.durationMicros ?? 0,
    );
  });
}
