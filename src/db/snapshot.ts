/*
Brave profile discovery and the read-only cached copy of each profile's History DB.
Brave locks History while running, so queries never touch it: the file (plus -wal / -journal
sidecars) is copied to the cache dir when the original is newer than the cached copy.
*/

import { existsSync, readdirSync, readFileSync } from "node:fs";
import { chmod, copyFile, mkdir, mkdtemp, readdir, rename, rm, stat, utimes } from "node:fs/promises";
import { homedir } from "node:os";
import { basename, dirname, join, resolve, sep } from "node:path";
import { openSqlite } from "@slimr/sqlite";

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
export async function ensureCachedHistory(profile: BraveProfile): Promise<string> {
  const root = resolve(braveRoot());
  const cacheDir = resolve(cacheRoot(), profile.dir);
  if (`${cacheDir}${sep}`.startsWith(`${root}${sep}`)) {
    throw new Error(`Refusing to cache inside the Brave data dir: ${cacheDir}`);
  }
  const source = join(root, profile.dir, "History");
  const cached = join(cacheDir, "History");
  const sourceMtimeMs = Math.max(
    ...(await Promise.all(
      ["", ...SIDECARS].map(async (suffix) => {
        try {
          return (await stat(source + suffix)).mtimeMs;
        } catch {
          return 0;
        }
      }),
    )),
  );
  let cachedMtimeMs = -1;
  try {
    cachedMtimeMs = (await stat(cached)).mtimeMs;
  } catch {}
  if (sourceMtimeMs <= cachedMtimeMs) {
    try {
      await lockDown(cacheDir);
    } catch {
      // cacheDir doesn't exist or couldn't be locked down
    }
    return cached;
  }

  // mkdtemp creates the staging dir 0o700 with an unpredictable name next to the final dir.
  await mkdir(dirname(cacheDir), { recursive: true, mode: 0o700 });
  await chmod(dirname(cacheDir), 0o700);
  const staging = await mkdtemp(join(dirname(cacheDir), `${basename(cacheDir)}.tmp-`));
  try {
    for (const suffix of ["", ...SIDECARS]) {
      try {
        await copyFile(source + suffix, join(staging, `History${suffix}`));
      } catch (err: unknown) {
        if ((err as NodeJS.ErrnoException).code !== "ENOENT") throw err;
      }
    }
    // Open the copy read-write once so SQLite folds any WAL or hot journal into it; later reads are read-only.
    const db = openSqlite(join(staging, "History"), { pragmas: false });
    try {
      await db.get("SELECT count(*) FROM sqlite_master");
    } finally {
      await db.close();
    }

    await lockDown(staging);
    await rm(cacheDir, { recursive: true, force: true });
    await rename(staging, cacheDir);
    const mtime = new Date(sourceMtimeMs);
    await utimes(cached, mtime, mtime);
    return cached;
  } catch (err) {
    await rm(staging, { recursive: true, force: true }).catch(() => {});
    throw err;
  }
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

/** Browsing history is private: owner-only dir (0o700) and files (0o600), applied to existing copies too. */
async function lockDown(dir: string): Promise<void> {
  await chmod(dir, 0o700);
  for (const entry of await readdir(dir)) await chmod(join(dir, entry), 0o600);
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
