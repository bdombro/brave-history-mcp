import assert from "node:assert/strict";
import { describe, test } from "node:test";
import { chromeMicrosToUnixMs, formatLocalIso, localDayStartMs, unixMsToChromeMicros } from "./time.ts";

describe("chromium timestamps", () => {
  test("epoch offset: 1601-01-01 and 1970-01-01", () => {
    assert.equal(chromeMicrosToUnixMs(11_644_473_600_000_000n), 0);
    assert.equal(chromeMicrosToUnixMs(0n), Date.UTC(1601, 0, 1));
  });

  test("round-trips a real instant beyond Number.MAX_SAFE_INTEGER micros", () => {
    const ms = Date.parse("2026-11-07T23:23:26.123Z");
    const chrome = unixMsToChromeMicros(ms);
    assert.ok(chrome > BigInt(Number.MAX_SAFE_INTEGER));
    assert.equal(chrome, 13_438_567_406_123_000n);
    assert.equal(chromeMicrosToUnixMs(chrome), ms);
  });
});

describe("Europe/Dublin local time", () => {
  test("winter is +00:00, summer is +01:00", () => {
    assert.equal(formatLocalIso(Date.parse("2026-01-15T09:30:05Z"), "Europe/Dublin"), "2026-01-15T09:30:05+00:00");
    assert.equal(formatLocalIso(Date.parse("2026-07-01T12:00:00Z"), "Europe/Dublin"), "2026-07-01T13:00:00+01:00");
    assert.equal(formatLocalIso(Date.parse("2026-07-01T12:00:00Z"), "America/New_York"), "2026-07-01T08:00:00-04:00");
  });

  test("local day start honours the offset", () => {
    assert.equal(localDayStartMs("2026-01-15", "Europe/Dublin"), Date.UTC(2026, 0, 15));
    assert.equal(localDayStartMs("2026-07-01", "Europe/Dublin"), Date.UTC(2026, 5, 30, 23));
    assert.throws(() => localDayStartMs("2026-7-1", "Europe/Dublin"), /YYYY-MM-DD/);
  });
});
