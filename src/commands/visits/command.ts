/*
visits — every visit to one URL (or every URL under a prefix) with times and transition types.
*/

import { command, OptionKind } from "argsbarg";
import { z } from "zod";
import { queryVisits } from "../../db/index.ts";
import { FILTER_OPTIONS, filterFields, urlClean } from "../../schemas.ts";

/** Input for `visits`. */
export const VisitsInput = z.strictObject({
  url: z.string().min(1).describe("Exact page URL (a trailing / is also tried), or a URL prefix with prefix=true."),
  prefix: z.boolean().optional().describe("Match every URL starting with url instead of the exact URL."),
  ...filterFields,
});

/** Parsed `visits` input. */
export type VisitsInput = z.infer<typeof VisitsInput>;

/** Result of `visits`. */
export const VisitsOutput = z.strictObject({
  visits: z
    .array(
      z.strictObject({
        time: z.string().describe("Visit time, ISO 8601 Europe/Dublin local time with offset."),
        url: z.string().describe("Visited URL (query string and #fragment removed unless fullUrls is set)."),
        title: z.string().describe("Page title (may be empty)."),
        transition: z
          .string()
          .describe(
            "How the visit started: link, typed, auto_bookmark, auto_subframe, manual_subframe, generated, auto_toplevel, form_submit, reload, keyword, keyword_generated.",
          ),
        durationSeconds: z.number().describe("Time the page was open, in seconds (0 when unknown)."),
        profile: z.string().describe("Brave profile directory the visit came from."),
      }),
    )
    .describe("Visits, newest first."),
});

/** `visits` payload. */
export type VisitsOutput = z.infer<typeof VisitsOutput>;

/** Lists every visit to a URL or URL prefix. */
export const visitsCommand = command({
  key: "visits",
  description:
    "List every Brave visit to a URL (or URL prefix) with times and transition types. URLs come back without query strings or #fragments unless fullUrls is set.",
  notes: "Example: {argsbarg:program} visits https://github.com/bdombro/ --prefix --limit 100",
  options: [
    {
      name: "prefix",
      description: "Match every URL starting with the given URL instead of the exact URL.",
      kind: OptionKind.Presence,
    },
    ...FILTER_OPTIONS,
  ],
  positionals: [
    {
      name: "url",
      description: "Exact page URL (a trailing / is also tried), or a URL prefix with --prefix.",
      kind: OptionKind.String,
      argMin: 1,
      argMax: 1,
    },
  ],
  inputSchema: VisitsInput,
  outputSchema: VisitsOutput,
  mcpTool: { annotations: { readOnlyHint: true, openWorldHint: false } },
  handler: (ctx) => {
    const { url, prefix, from, to, profile, limit } = ctx.inputs;
    const full = ctx.inputs.fullUrls;
    const out: VisitsOutput = {
      visits: queryVisits({ url, prefix: prefix ?? false, from, to, profile, limit }).map((v) => ({
        ...v,
        url: urlClean(v.url, full),
      })),
    };
    if (ctx.invocation !== "cli") {
      return out;
    }
    if (ctx.hasFlag("json")) {
      console.log(JSON.stringify(out, null, 2));
    } else if (out.visits.length === 0) {
      console.log("No visits found.");
    } else {
      for (const v of out.visits) {
        console.log(
          `${v.time.slice(0, 19).replace("T", " ")}  ${v.transition.padEnd(12)} ${v.durationSeconds}s  ${v.url}`,
        );
      }
    }
  },
});
