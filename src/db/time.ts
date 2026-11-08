/*
Chromium timestamp conversion and local-time formatting.
Chromium stores times as microseconds since 1601-01-01 UTC; values exceed Number.MAX_SAFE_INTEGER,
so they cross the SQLite boundary as bigint (params) or as unix milliseconds computed in SQL (results).
*/

/** Microseconds between 1601-01-01 and 1970-01-01 (the Chromium epoch offset). */
export const CHROME_EPOCH_OFFSET_MICROS = 11_644_473_600_000_000n;

/** Default time zone for output and date-only filters. */
export const DEFAULT_TIME_ZONE = "Europe/Dublin";

/** Converts a Chromium timestamp (µs since 1601) to unix milliseconds. */
export function chromeMicrosToUnixMs(chromeMicros: bigint): number {
  return Number((chromeMicros - CHROME_EPOCH_OFFSET_MICROS) / 1000n);
}

/**
 * Formats unix milliseconds as an ISO 8601 local time with offset in `timeZone`
 * (e.g. `2026-07-01T13:00:00+01:00`).
 */
export function formatLocalIso(unixMs: number, timeZone: string): string {
  const wall = wallClock(unixMs, timeZone);
  const offsetMin = Math.round((wall.asUtcMs - Math.floor(unixMs / 1000) * 1000) / 60_000);
  const sign = offsetMin < 0 ? "-" : "+";
  const abs = Math.abs(offsetMin);
  return `${wall.date}T${wall.time}${sign}${pad(Math.floor(abs / 60))}:${pad(abs % 60)}`;
}

/** Converts unix milliseconds to a Chromium timestamp (µs since 1601). */
export function unixMsToChromeMicros(unixMs: number): bigint {
  return BigInt(Math.trunc(unixMs)) * 1000n + CHROME_EPOCH_OFFSET_MICROS;
}

/** Unix milliseconds of local midnight that starts `date` (`YYYY-MM-DD`) in `timeZone`. */
export function localDayStartMs(date: string, timeZone: string): number {
  const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(date);
  if (!match) {
    throw new Error(`Invalid date (expected YYYY-MM-DD): ${date}`);
  }
  const guess = Date.UTC(Number(match[1]), Number(match[2]) - 1, Number(match[3]));
  const first = guess - offsetMs(guess, timeZone);
  const second = guess - offsetMs(first, timeZone);
  return second;
}

/** Zone offset (local minus UTC) in milliseconds at `unixMs`. */
function offsetMs(unixMs: number, timeZone: string): number {
  return wallClock(unixMs, timeZone).asUtcMs - Math.floor(unixMs / 1000) * 1000;
}

function pad(n: number): string {
  return String(n).padStart(2, "0");
}

/** Local wall-clock parts at `unixMs` in `timeZone`, plus those parts read back as if UTC. */
function wallClock(unixMs: number, timeZone: string): { asUtcMs: number; date: string; time: string } {
  const parts = Object.fromEntries(
    new Intl.DateTimeFormat("en-CA", {
      timeZone,
      year: "numeric",
      month: "2-digit",
      day: "2-digit",
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
      hourCycle: "h23",
    })
      .formatToParts(new Date(unixMs))
      .map((p) => [p.type, p.value]),
  );
  const asUtcMs = Date.UTC(
    Number(parts.year),
    Number(parts.month) - 1,
    Number(parts.day),
    Number(parts.hour),
    Number(parts.minute),
    Number(parts.second),
  );
  return {
    asUtcMs,
    date: `${parts.year}-${parts.month}-${parts.day}`,
    time: `${parts.hour}:${parts.minute}:${parts.second}`,
  };
}
