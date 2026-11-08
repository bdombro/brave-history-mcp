/*
Zod fields and CLI options shared by the history commands.
*/

import { OptionKind, ValueFormat } from "argsbarg";
import { z } from "zod";

const DATE = /^\d{4}-\d{2}-\d{2}$/;

/** Shared CLI options: date range, profile, limit, json. */
export const FILTER_OPTIONS = [
  {
    name: "from",
    description: "First day included, YYYY-MM-DD (Europe/Dublin local).",
    kind: OptionKind.String,
    format: ValueFormat.Date,
  },
  {
    name: "to",
    description: "Last day included, YYYY-MM-DD (Europe/Dublin local).",
    kind: OptionKind.String,
    format: ValueFormat.Date,
  },
  {
    name: "profile",
    description: "Brave profile directory or display name (e.g. Default, Profile 1). All profiles when omitted.",
    kind: OptionKind.String,
  },
  {
    name: "limit",
    description: "Maximum rows to return (1-500, default 25).",
    kind: OptionKind.Number,
  },
  {
    name: "fullUrls",
    description: "Keep query strings and #fragments in returned URLs (stripped by default).",
    kind: OptionKind.Presence,
  },
  {
    name: "json",
    description: "Emit JSON.",
    kind: OptionKind.Presence,
  },
] as const;

/** Zod fields matching {@link FILTER_OPTIONS}. */
export const filterFields = {
  from: z.string().regex(DATE).optional().describe("First day included, YYYY-MM-DD (Europe/Dublin local)."),
  to: z.string().regex(DATE).optional().describe("Last day included, YYYY-MM-DD (Europe/Dublin local)."),
  profile: z
    .string()
    .optional()
    .describe("Brave profile directory or display name (e.g. Default, Profile 1). All profiles when omitted."),
  limit: z.number().int().min(1).max(500).default(25).describe("Maximum rows to return (1-500, default 25)."),
  fullUrls: z
    .boolean()
    .optional()
    .describe(
      "Keep query strings and #fragments in returned URLs. Off by default because they often hold tokens and ids; only set it when you need the exact URL.",
    ),
  json: z.boolean().optional().describe("CLI only: print JSON instead of text. MCP always returns JSON."),
};

/** One page (unique URL) from Brave history. */
export const PageRowSchema = z.strictObject({
  title: z.string().describe("Page title (may be empty)."),
  url: z.string().describe("Page URL (query string and #fragment removed unless fullUrls is set)."),
  lastVisit: z.string().describe("Last visit in range, ISO 8601 Europe/Dublin local time with offset."),
  visitCount: z.number().describe("Total visits Brave recorded for the URL."),
  profile: z.string().describe("Brave profile directory the row came from."),
});

/** Result of `search` and `list`. */
export const PagesOutput = z.strictObject({
  pages: z.array(PageRowSchema).describe("Pages, newest last visit first."),
});

/** `search` / `list` payload. */
export type PagesOutput = z.infer<typeof PagesOutput>;

/** Drops the query string and #fragment from `url` unless `full` is set. */
export function urlClean(url: string, full: boolean | undefined): string {
  if (full) return url;
  const cut = url.search(/[?#]/);
  return cut === -1 ? url : url.slice(0, cut);
}

/** Human-readable lines for a page list. */
export function formatPages(out: PagesOutput): string {
  if (out.pages.length === 0) {
    return "No matching pages.";
  }
  return out.pages
    .map(
      (p) =>
        `${p.lastVisit.slice(0, 16).replace("T", " ")}  ${p.title || "(untitled)"}\n    ${p.url}  (${p.visitCount} visits, ${p.profile})`,
    )
    .join("\n");
}
