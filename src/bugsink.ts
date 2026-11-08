/*
brave-history's Bugsink reporter (Sentry-compatible), built on argsbarg's createErrorReporter.
The app passes it as `errorReporter`, so command failures are reported automatically. Off when BUGSINK_DSN is set to an empty string.
*/
import { createErrorReporter } from "argsbarg";

/** brave-history's Bugsink project DSN (send-only key); override with BUGSINK_DSN, "" disables. */
const DEFAULT_DSN = "https://b6c38ca7c9f14e9ba4157d7b6ac46dbc@bugsink.btek.cc/5";
/** The app's error reporter. */
export const errors = createErrorReporter({ dsn: DEFAULT_DSN, env: "BUGSINK_DSN" });
