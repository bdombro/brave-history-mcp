---
id: brave-history
name: brave-history
description: Searches Brian's Brave browser history (search, list, visits). Use when recalling a site, product, article, doc or video he looked at, when he visited a page, or what he browsed on a domain or day.
enabled: true
---

# brave-history

Read-only access to Brave browser history (all profiles). Prefer the MCP tools (`search`, `list`, `visits`); the CLI is `brave-history <command>` with the same arguments.

## Which tool

- **`search`**: you have keywords ("that standing desk on Amazon"). `words` are ANDed over title and URL, so use 1-3 distinctive words; add `domain`, `from` / `to` (YYYY-MM-DD, inclusive) to narrow it.
- **`list`**: no keywords, just "what did I look at on youtube.com last week" or "pages from yesterday".
- **`visits`**: you already have a URL and need when or how often it was visited. Set `prefix: true` for a whole section (`https://github.com/bdombro/`).

## Tips

- Times are Europe/Dublin local ISO with offset. `lastVisit` is the latest visit inside the date range.
- No hits: drop a word, try a synonym or the site's name, or remove the date range. Titles can be empty, so URLs carry many matches.
- `profile` takes a directory (`Default`, `Profile 1`) or display name; omit it to search every profile.
- Results are page rows, not page content. Open the URL with a browser tool if you need the content.
