#pragma once

#include "reflect.hpp"

#include <optional>
#include <string>
#include <vector>

namespace brave_history {

using slimr::Field;
using slimr::OptField;

inline std::string urlClean(const std::string& url, bool full) {
    if (full) return url;
    size_t cut = url.find_first_of("?#");
    if (cut == std::string::npos) return url;
    return url.substr(0, cut);
}

struct PageRow {
    std::string title;
    std::string url;
    std::string lastVisit;
    int64_t visitCount = 0;
    std::string profile;

    bool operator==(const PageRow& o) const = default;
};

struct PagesOutput {
    std::vector<PageRow> pages;

    bool operator==(const PagesOutput& o) const = default;
};

struct VisitRow {
    std::string time;
    std::string url;
    std::string title;
    std::string transition;
    double durationSeconds = 0.0;
    std::string profile;

    bool operator==(const VisitRow& o) const = default;
};

struct VisitsOutput {
    std::vector<VisitRow> visits;

    bool operator==(const VisitsOutput& o) const = default;
};

struct StatusOutput {
    std::string version = "2.0.0";

    bool operator==(const StatusOutput& o) const = default;
};

struct StatusInput {
    OptField<std::optional<bool>, "CLI only: print JSON instead of text. MCP always returns JSON."> json;
};

struct SearchInput {
    Field<std::vector<std::string>, "Keywords; every word must appear in the title or URL (case-insensitive substring)."> words;
    OptField<std::optional<std::string>, "Only pages on this host or its subdomains (e.g. amazon.com matches www.amazon.com)."> domain;
    OptField<std::optional<std::string>, "First day included, YYYY-MM-DD (Europe/Dublin local)."> from;
    OptField<std::optional<std::string>, "Last day included, YYYY-MM-DD (Europe/Dublin local)."> to;
    OptField<std::optional<std::string>, "Brave profile directory or display name (e.g. Default, Profile 1). All profiles when omitted."> profile;
    OptField<int, "Maximum rows to return (1-500, default 25)."> limit{25};
    OptField<std::optional<bool>, "Keep query strings and #fragments in returned URLs. Off by default because they often hold tokens and ids; only set it when you need the exact URL."> fullUrls;
    OptField<std::optional<bool>, "CLI only: print JSON instead of text. MCP always returns JSON."> json;
};

struct ListInput {
    OptField<std::optional<std::string>, "Only pages on this host or its subdomains (e.g. amazon.com matches www.amazon.com)."> domain;
    OptField<std::optional<std::string>, "First day included, YYYY-MM-DD (Europe/Dublin local)."> from;
    OptField<std::optional<std::string>, "Last day included, YYYY-MM-DD (Europe/Dublin local)."> to;
    OptField<std::optional<std::string>, "Brave profile directory or display name (e.g. Default, Profile 1). All profiles when omitted."> profile;
    OptField<int, "Maximum rows to return (1-500, default 25)."> limit{25};
    OptField<std::optional<bool>, "Keep query strings and #fragments in returned URLs. Off by default because they often hold tokens and ids; only set it when you need the exact URL."> fullUrls;
    OptField<std::optional<bool>, "CLI only: print JSON instead of text. MCP always returns JSON."> json;
};

struct VisitsInput {
    Field<std::string, "Exact page URL (a trailing / is also tried), or a URL prefix with prefix=true."> url;
    OptField<std::optional<bool>, "Match every URL starting with url instead of the exact URL."> prefix;
    OptField<std::optional<std::string>, "First day included, YYYY-MM-DD (Europe/Dublin local)."> from;
    OptField<std::optional<std::string>, "Last day included, YYYY-MM-DD (Europe/Dublin local)."> to;
    OptField<std::optional<std::string>, "Brave profile directory or display name (e.g. Default, Profile 1). All profiles when omitted."> profile;
    OptField<int, "Maximum rows to return (1-500, default 25)."> limit{25};
    OptField<std::optional<bool>, "Keep query strings and #fragments in returned URLs. Off by default because they often hold tokens and ids; only set it when you need the exact URL."> fullUrls;
    OptField<std::optional<bool>, "CLI only: print JSON instead of text. MCP always returns JSON."> json;
};

} // namespace brave_history
