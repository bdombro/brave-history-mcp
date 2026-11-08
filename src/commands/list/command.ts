/*
list — recent pages, optionally limited to one domain and a date range.
*/

import { command, OptionKind } from "argsbarg";
import { z } from "zod";
import { queryPages } from "../../db/index.ts";
import { FILTER_OPTIONS, filterFields, formatPages, PagesOutput, urlClean } from "../../schemas.ts";

/** Input for `list`. */
export const ListInput = z.strictObject({
  domain: z
    .string()
    .optional()
    .describe("Only pages on this host or its subdomains (e.g. amazon.com matches www.amazon.com)."),
  ...filterFields,
});

/** Parsed `list` input. */
export type ListInput = z.infer<typeof ListInput>;

/** Lists recently visited pages. */
export const listCommand = command({
  key: "list",
  description:
    "List recently visited pages in Brave, newest first. URLs come back without query strings or #fragments unless fullUrls is set.",
  notes: "Example: {argsbarg:program} list --domain youtube.com --from 2026-11-01 --limit 50",
  options: [
    {
      name: "domain",
      description: "Only pages on this host or its subdomains (e.g. amazon.com matches www.amazon.com).",
      kind: OptionKind.String,
    },
    ...FILTER_OPTIONS,
  ],
  inputSchema: ListInput,
  outputSchema: PagesOutput,
  mcpTool: { annotations: { readOnlyHint: true, openWorldHint: false } },
  handler: (ctx) => {
    const { domain, from, to, profile, limit } = ctx.inputs;
    const full = ctx.inputs.fullUrls;
    const out: PagesOutput = {
      pages: queryPages({ words: [], domain, from, to, profile, limit }).map((p) => ({
        ...p,
        url: urlClean(p.url, full),
      })),
    };
    if (ctx.invocation === "cli") {
      console.log(ctx.hasFlag("json") ? JSON.stringify(out, null, 2) : formatPages(out));
      return;
    }
    return out;
  },
});
