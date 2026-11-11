#pragma once

#include "db.hpp"
#include "schemas.hpp"
#include "mcp.hpp"

#include <stdexcept>

namespace brave_history {

inline slimr::PluginApp make_brave_history_app() {
    slimr::PluginApp app("brave-history-mcp", "2.0.0");
    app.description = "Search Brave browser history (titles, URLs, visits) over MCP.";
    app.instructions =
        "Your Brave browser history (read-only). Use it to recall sites, products, articles, or docs you looked at: "
        "search by keywords, list recent pages by domain/date, or get every visit to a URL.";
    app.setBugsink("");

    app.tool<SearchInput>(
        "search",
        "Search Brave history by keywords in page titles and URLs (matching uses the full stored URL). "
        "URLs come back without query strings or #fragments unless fullUrls is set.",
        slimr::ToolAnnotations{.readOnlyHint = true, .openWorldHint = false},
        [](const SearchInput& in) -> PagesOutput {
            if (in.words.get().empty()) {
                throw std::runtime_error("words must contain at least 1 keyword");
            }
            bool full = in.fullUrls.get().value_or(false);
            PageQuery q;
            q.words = in.words.get();
            q.domain = in.domain.get();
            q.from = in.from.get();
            q.to = in.to.get();
            q.profile = in.profile.get();
            q.limit = in.limit.get();

            auto pages = queryPages(q);
            for (auto& p : pages) {
                p.url = urlClean(p.url, full);
            }
            return PagesOutput{.pages = std::move(pages)};
        }
    );

    app.tool<ListInput>(
        "list",
        "List recently visited pages in Brave, newest first. URLs come back without query strings or #fragments unless fullUrls is set.",
        slimr::ToolAnnotations{.readOnlyHint = true, .openWorldHint = false},
        [](const ListInput& in) -> PagesOutput {
            bool full = in.fullUrls.get().value_or(false);
            PageQuery q;
            q.words = {};
            q.domain = in.domain.get();
            q.from = in.from.get();
            q.to = in.to.get();
            q.profile = in.profile.get();
            q.limit = in.limit.get();

            auto pages = queryPages(q);
            for (auto& p : pages) {
                p.url = urlClean(p.url, full);
            }
            return PagesOutput{.pages = std::move(pages)};
        }
    );

    app.tool<VisitsInput>(
        "visits",
        "List every Brave visit to a URL (or URL prefix) with times and transition types. "
        "URLs come back without query strings or #fragments unless fullUrls is set.",
        slimr::ToolAnnotations{.readOnlyHint = true, .openWorldHint = false},
        [](const VisitsInput& in) -> VisitsOutput {
            bool full = in.fullUrls.get().value_or(false);
            VisitQuery q;
            q.url = in.url.get();
            q.prefix = in.prefix.get().value_or(false);
            q.from = in.from.get();
            q.to = in.to.get();
            q.profile = in.profile.get();
            q.limit = in.limit.get();

            auto visits = queryVisits(q);
            for (auto& v : visits) {
                v.url = urlClean(v.url, full);
            }
            return VisitsOutput{.visits = std::move(visits)};
        }
    );

    app.tool<StatusInput>(
        "status",
        "Show app version.",
        [](const StatusInput&) -> StatusOutput {
            return StatusOutput{.version = "2.0.0"};
        }
    );

    return app;
}

} // namespace brave_history
