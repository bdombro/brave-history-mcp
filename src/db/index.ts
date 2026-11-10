/*
Read-only queries over the cached copies of Brave's Chromium History DBs (`urls`, `visits`).
Each query runs per selected profile, then results are merged newest first.
*/

import { openSqlite, type SQLInputValue } from "@slimr/sqlite";
import { type BraveProfile, ensureCachedHistory, selectProfiles } from "./snapshot.ts";
import {
  CHROME_EPOCH_OFFSET_MICROS,
  DEFAULT_TIME_ZONE,
  formatLocalIso,
  localDayStartMs,
  unixMsToChromeMicros,
} from "./time.ts";

/** Filters shared by page and visit queries. */
export interface HistoryFilters {
  /** First local day included (`YYYY-MM-DD`). */
  from?: string;
  /** Maximum rows returned. */
  limit: number;
  /** Profile directory or display name; all profiles when omitted. */
  profile?: string;
  /** Last local day included (`YYYY-MM-DD`). */
  to?: string;
}

/** One page (unique URL) from history. */
export interface PageRow {
  lastVisit: string;
  profile: string;
  title: string;
  url: string;
  visitCount: number;
}

/** Page query: words ANDed over title and URL, optional host filter. */
export interface PageQuery extends HistoryFilters {
  /** Host to match, including subdomains (`amazon.com` matches `www.amazon.com`). */
  domain?: string;
  /** Words that must all appear in the title or URL (case-insensitive). */
  words: string[];
}

/** One visit to a URL. */
export interface VisitRow {
  durationSeconds: number;
  profile: string;
  time: string;
  title: string;
  transition: string;
  url: string;
}

/** Visit query for one URL or every URL under a prefix. */
export interface VisitQuery extends HistoryFilters {
  /** Treat `url` as a prefix instead of an exact URL. */
  prefix: boolean;
  /** Exact URL (a trailing `/` is also tried) or URL prefix. */
  url: string;
}

/** Output time zone; override with `BRAVE_HISTORY_TZ`. */
export function timeZone(): string {
  return process.env.BRAVE_HISTORY_TZ ?? DEFAULT_TIME_ZONE;
}

/** Pages matching `query`, newest last visit first. Last visit is the latest one inside the date range. */
export async function queryPages(query: PageQuery): Promise<PageRow[]> {
  const range = chromeRange(query);
  const where = ["u.hidden = 0"];
  const params: SQLInputValue[] = [];
  const joinParams: SQLInputValue[] = [];
  let join = "LEFT JOIN visits v ON v.url = u.id";
  let having = "lastChrome > 0";
  if (range) {
    join += " AND v.visit_time >= ? AND v.visit_time < ?";
    joinParams.push(range.start, range.end);
    having = "MAX(v.visit_time) IS NOT NULL OR (u.last_visit_time >= ? AND u.last_visit_time < ?)";
  }
  for (const word of query.words) {
    where.push(`(u.title LIKE ? ESCAPE '\\' OR u.url LIKE ? ESCAPE '\\')`);
    const pattern = `%${escapeLike(word)}%`;
    params.push(pattern, pattern);
  }
  const domain = normalizeDomain(query.domain);
  if (domain) {
    const d = escapeLike(domain);
    where.push(
      `(${HOST_SQL} = ? OR ${HOST_SQL} LIKE ? ESCAPE '\\' OR ${HOST_SQL} LIKE ? ESCAPE '\\' OR ${HOST_SQL} LIKE ? ESCAPE '\\')`,
    );
    params.push(domain, `%.${d}`, `${d}:%`, `%.${d}:%`);
  }
  const sql = `
    SELECT u.url, u.title, u.visit_count AS visitCount,
      COALESCE(MAX(v.visit_time), u.last_visit_time) AS lastChrome,
      (COALESCE(MAX(v.visit_time), u.last_visit_time) - ${CHROME_EPOCH_OFFSET_MICROS}) / 1000 AS lastMs
    FROM urls u ${join}
    WHERE ${where.join(" AND ")}
    GROUP BY u.id
    HAVING ${having}
    ORDER BY lastChrome DESC
    LIMIT ?`;
  const allParams = [...joinParams, ...params, ...(range ? [range.start, range.end] : []), query.limit];
  const tz = timeZone();
  return await acrossProfiles(query, sql, allParams, (row, profile) => ({
    title: String(row.title ?? ""),
    url: String(row.url),
    lastVisit: formatLocalIso(Number(row.lastMs), tz),
    visitCount: Number(row.visitCount),
    profile: profile.dir,
  }));
}

/** Visits to `query.url` (or URLs under it), newest first. */
export async function queryVisits(query: VisitQuery): Promise<VisitRow[]> {
  const range = chromeRange(query);
  const where: string[] = [];
  const params: SQLInputValue[] = [];
  if (query.prefix) {
    where.push(`u.url LIKE ? ESCAPE '\\'`);
    params.push(`${escapeLike(query.url)}%`);
  } else {
    where.push("(u.url = ? OR u.url = ?)");
    params.push(query.url, `${query.url}/`);
  }
  if (range) {
    where.push("v.visit_time >= ? AND v.visit_time < ?");
    params.push(range.start, range.end);
  }
  const sql = `
    SELECT u.url, u.title, v.visit_time AS timeChrome,
      (v.visit_time - ${CHROME_EPOCH_OFFSET_MICROS}) / 1000 AS timeMs,
      v.transition & 255 AS coreTransition, v.visit_duration AS durationMicros
    FROM visits v JOIN urls u ON u.id = v.url
    WHERE ${where.join(" AND ")}
    ORDER BY v.visit_time DESC
    LIMIT ?`;
  const tz = timeZone();
  return await acrossProfiles(query, sql, [...params, query.limit], (row, profile) => ({
    time: formatLocalIso(Number(row.timeMs), tz),
    url: String(row.url),
    title: String(row.title ?? ""),
    transition: TRANSITIONS[Number(row.coreTransition)] ?? `unknown_${row.coreTransition}`,
    durationSeconds: Math.round(Number(row.durationMicros ?? 0) / 100_000) / 10,
    profile: profile.dir,
  }));
}

/** Host part of `u.url` (lowercased, port kept) for domain filtering. */
const HOST_SQL = `lower(CASE WHEN instr(substr(u.url, instr(u.url, '://') + 3), '/') > 0
  THEN substr(substr(u.url, instr(u.url, '://') + 3), 1, instr(substr(u.url, instr(u.url, '://') + 3), '/') - 1)
  ELSE substr(u.url, instr(u.url, '://') + 3) END)`;

/** Chromium core transition types (`transition & 0xFF`). */
const TRANSITIONS = [
  "link",
  "typed",
  "auto_bookmark",
  "auto_subframe",
  "manual_subframe",
  "generated",
  "auto_toplevel",
  "form_submit",
  "reload",
  "keyword",
  "keyword_generated",
];

/**
 * Runs `sql` against each selected profile's cached History copy (read-only), maps rows, and
 * merges them by the row's Chromium time column (`lastChrome` / `timeChrome`), newest first.
 */
async function acrossProfiles<T>(
  filters: HistoryFilters,
  sql: string,
  params: SQLInputValue[],
  toRow: (row: Record<string, unknown>, profile: BraveProfile) => T,
): Promise<T[]> {
  const merged: { chrome: bigint; row: T }[] = [];
  for (const profile of selectProfiles(filters.profile)) {
    const cachedPath = await ensureCachedHistory(profile);
    const db = openSqlite(cachedPath, { readOnly: true, readBigInts: true });
    try {
      const rows = await db.all<Record<string, unknown>>(sql, params);
      for (const row of rows) {
        const chrome = (row.lastChrome ?? row.timeChrome) as bigint;
        merged.push({ chrome, row: toRow(row, profile) });
      }
    } finally {
      await db.close();
    }
  }
  merged.sort((a, b) => (a.chrome === b.chrome ? 0 : a.chrome < b.chrome ? 1 : -1));
  return merged.slice(0, filters.limit).map((m) => m.row);
}

/** Chromium bounds for the inclusive local-day range, or undefined when no dates are given. */
function chromeRange(filters: HistoryFilters): { end: bigint; start: bigint } | undefined {
  if (!filters.from && !filters.to) {
    return undefined;
  }
  const tz = timeZone();
  const start = filters.from ? unixMsToChromeMicros(localDayStartMs(filters.from, tz)) : 0n;
  const end = filters.to ? unixMsToChromeMicros(localDayStartMs(nextDay(filters.to), tz)) : 2n ** 62n;
  return { start, end };
}

function escapeLike(text: string): string {
  return text.replace(/[\\%_]/g, (c) => `\\${c}`);
}

function nextDay(date: string): string {
  const d = new Date(`${date}T00:00:00Z`);
  d.setUTCDate(d.getUTCDate() + 1);
  return d.toISOString().slice(0, 10);
}

/** `https://www.Example.com/x` or `www.example.com` → `www.example.com`. */
function normalizeDomain(domain: string | undefined): string | undefined {
  const trimmed = domain?.trim().toLowerCase();
  if (!trimmed) {
    return undefined;
  }
  return trimmed.replace(/^[a-z][a-z0-9+.-]*:\/\//, "").split(/[/?#]/)[0];
}
