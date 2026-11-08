/*
Brave profile discovery and the read-only cached copy of each profile's History DB.
Brave locks History while running, so queries never touch it: the file (plus -wal / -journal
sidecars) is copied to the cache dir when the original is newer than the cached copy.
*/

import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  renameSync,
  rmSync,
  statSync,
  utimesSync,
} from "node:fs";
import { homedir } from "node:os";
import { join, resolve, sep } from "node:path";
import { DatabaseSync } from "node:sqlite";

/** One Brave profile that has a History DB. */
export interface BraveProfile {
  /** Profile directory name (`Default`, `Profile 1`, …). */
  dir: string;
  /** Display name from Brave's `Local State`, when known. */
  name?: string;
}

/** Brave user-data dir; override with `BRAVE_HISTORY_ROOT`. */
export function braveRoot(): string {
  return (
    process.env.BRAVE_HISTORY_ROOT ??
    join(homedir(), "Library", "Application Support", "BraveSoftware", "Brave-Browser")
  );
}

/** Cache dir for copied History files; override with `BRAVE_HISTORY_CACHE`. */
export function cacheRoot(): string {
  return process.env.BRAVE_HISTORY_CACHE ?? join(homedir(), ".local", "share", "brave-history");
}

/**
 * Returns the path of an up-to-date cached copy of `profile`'s History DB.
 * Copies when the original (or a sidecar) has a newer mtime than the cached copy; the copy's
 * mtime is then set to the original's so the next call can skip the copy.
 */
export function ensureCachedHistory(profile: BraveProfile): string {
  const root = resolve(braveRoot());
  const cacheDir = resolve(cacheRoot(), profile.dir);
  if (`${cacheDir}${sep}`.startsWith(`${root}${sep}`)) {
    throw new Error(`Refusing to cache inside the Brave data dir: ${cacheDir}`);
  }
  const source = join(root, profile.dir, "History");
  const cached = join(cacheDir, "History");
  const sourceMtimeMs = Math.max(
    ...["", ...SIDECARS].map((suffix) => (existsSync(source + suffix) ? statSync(source + suffix).mtimeMs : 0)),
  );
  const cachedMtimeMs = existsSync(cached) ? statSync(cached).mtimeMs : -1;
  if (sourceMtimeMs <= cachedMtimeMs) {
    return cached;
  }

  const staging = `${cacheDir}.tmp-${process.pid}`;
  rmSync(staging, { recursive: true, force: true });
  mkdirSync(staging, { recursive: true });
  for (const suffix of ["", ...SIDECARS]) {
    if (existsSync(source + suffix)) {
      copyFileSync(source + suffix, join(staging, `History${suffix}`));
    }
  }
  // Open the copy read-write once so SQLite folds any WAL or hot journal into it; later reads are read-only.
  const db = new DatabaseSync(join(staging, "History"));
  db.prepare("SELECT count(*) FROM sqlite_master").get();
  db.close();

  rmSync(cacheDir, { recursive: true, force: true });
  renameSync(staging, cacheDir);
  const mtime = new Date(sourceMtimeMs);
  utimesSync(cached, mtime, mtime);
  return cached;
}

/** Lists profiles under {@link braveRoot} that have a History DB (`Default` first). */
export function listProfiles(): BraveProfile[] {
  const root = braveRoot();
  if (!existsSync(root)) {
    throw new Error(`Brave data dir not found: ${root} (set BRAVE_HISTORY_ROOT to override)`);
  }
  const names = profileNames(root);
  return readdirSync(root)
    .filter((dir) => (dir === "Default" || /^Profile \d+$/.test(dir)) && existsSync(join(root, dir, "History")))
    .sort((a, b) => (a === "Default" ? -1 : b === "Default" ? 1 : a.localeCompare(b, "en", { numeric: true })))
    .map((dir) => (names[dir] ? { dir, name: names[dir] } : { dir }));
}

/** Filters profiles by directory or display name (case-insensitive); errors when nothing matches. */
export function selectProfiles(filter: string | undefined): BraveProfile[] {
  const all = listProfiles();
  if (!filter) {
    return all;
  }
  const needle = filter.toLowerCase();
  const matched = all.filter((p) => p.dir.toLowerCase() === needle || p.name?.toLowerCase() === needle);
  if (matched.length === 0) {
    const known = all.map((p) => (p.name ? `${p.dir} (${p.name})` : p.dir)).join(", ");
    throw new Error(`No Brave profile matches "${filter}". Known: ${known || "none"}`);
  }
  return matched;
}

const SIDECARS = ["-wal", "-journal"];

/** Profile display names keyed by directory, from `Local State` (empty when unreadable). */
function profileNames(root: string): Record<string, string> {
  try {
    const localState = JSON.parse(readFileSync(join(root, "Local State"), "utf8")) as {
      profile?: { info_cache?: Record<string, { name?: string }> };
    };
    const cache = localState.profile?.info_cache ?? {};
    return Object.fromEntries(
      Object.entries(cache).flatMap(([dir, info]) => (info.name ? [[dir, info.name] as const] : [])),
    );
  } catch {
    return {};
  }
}
