/*
search — keyword search over page titles and URLs (words ANDed).
*/

import { command, OptionKind } from "argsbarg";
import { z } from "zod";
import { queryPages } from "../../db/index.ts";
import { FILTER_OPTIONS, filterFields, formatPages, PagesOutput, urlClean } from "../../schemas.ts";

/** Input for `search`. */
export const SearchInput = z.strictObject({
  words: z
    .array(z.string())
    .min(1)
    .describe("Keywords; every word must appear in the title or URL (case-insensitive substring)."),
  domain: z
    .string()
    .optional()
    .describe("Only pages on this host or its subdomains (e.g. amazon.com matches www.amazon.com)."),
  ...filterFields,
});

/** Parsed `search` input. */
export type SearchInput = z.infer<typeof SearchInput>;

/** Searches history titles and URLs. */
export const searchCommand = command({
  key: "search",
  description:
    "Search Brave history by keywords in page titles and URLs (matching uses the full stored URL). URLs come back without query strings or #fragments unless fullUrls is set.",
  notes: "Example: {argsbarg:program} search standing desk --domain amazon.com --from 2026-10-01",
  options: [
    {
      name: "domain",
      description: "Only pages on this host or its subdomains (e.g. amazon.com matches www.amazon.com).",
      kind: OptionKind.String,
    },
    ...FILTER_OPTIONS,
  ],
  positionals: [
    {
      name: "words",
      description: "Keywords; every word must appear in the title or URL (case-insensitive substring).",
      kind: OptionKind.String,
      argMin: 1,
      argMax: 0,
    },
  ],
  inputSchema: SearchInput,
  outputSchema: PagesOutput,
  mcpTool: { annotations: { readOnlyHint: true, openWorldHint: false } },
  handler: (ctx) => {
    const { words, domain, from, to, profile, limit } = ctx.inputs;
    const full = ctx.inputs.fullUrls;
    const out: PagesOutput = {
      pages: queryPages({ words, domain, from, to, profile, limit }).map((p) => ({ ...p, url: urlClean(p.url, full) })),
    };
    if (ctx.invocation === "cli") {
      console.log(ctx.hasFlag("json") ? JSON.stringify(out, null, 2) : formatPages(out));
      return;
    }
    return out;
  },
});
