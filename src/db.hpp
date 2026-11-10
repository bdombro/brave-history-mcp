#pragma once

#include "schemas.hpp"
#include "snapshot.hpp"
#include "time.hpp"
#include "slimr/sqlite.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace brave_history {

struct PageQuery {
    std::vector<std::string> words;
    std::optional<std::string> domain;
    std::optional<std::string> from;
    std::optional<std::string> to;
    std::optional<std::string> profile;
    int limit = 25;
};

struct VisitQuery {
    std::string url;
    bool prefix = false;
    std::optional<std::string> from;
    std::optional<std::string> to;
    std::optional<std::string> profile;
    int limit = 25;
};

inline const std::string HOST_SQL =
    "lower(CASE WHEN instr(substr(u.url, instr(u.url, '://') + 3), '/') > 0 "
    "THEN substr(substr(u.url, instr(u.url, '://') + 3), 1, instr(substr(u.url, instr(u.url, '://') + 3), '/') - 1) "
    "ELSE substr(u.url, instr(u.url, '://') + 3) END)";

inline const std::vector<std::string> TRANSITIONS = {
    "link",
    "typed",
    "auto_bookmark",
    "auto_subframe",
    "manual_subframe",
    "generated",
    "auto_toplevel",
    "form_submit",
    "reload",
    "keyword",
    "keyword_generated",
};

inline std::string escapeLike(std::string_view text) {
    std::string res;
    res.reserve(text.size());
    for (char c : text) {
        if (c == '\\' || c == '%' || c == '_') {
            res.push_back('\\');
        }
        res.push_back(c);
    }
    return res;
}

inline std::optional<std::string> normalizeDomain(const std::optional<std::string>& domain) {
    if (!domain.has_value()) return std::nullopt;
    std::string s = *domain;
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    if (s.empty()) return std::nullopt;

    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    size_t scheme_pos = s.find("://");
    if (scheme_pos != std::string::npos) {
        bool valid_scheme = true;
        if (scheme_pos == 0 || !std::isalpha(static_cast<unsigned char>(s[0]))) {
            valid_scheme = false;
        } else {
            for (size_t i = 1; i < scheme_pos; ++i) {
                char c = s[i];
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '.' && c != '-') {
                    valid_scheme = false;
                    break;
                }
            }
        }
        if (valid_scheme) {
            s = s.substr(scheme_pos + 3);
        }
    }

    size_t end_pos = s.find_first_of("/?#");
    if (end_pos != std::string::npos) {
        s = s.substr(0, end_pos);
    }
    if (s.empty()) return std::nullopt;
    return s;
}

struct ChromeRange {
    int64_t start = 0;
    int64_t end = 0;
};

inline std::optional<ChromeRange> chromeRange(const std::optional<std::string>& from, const std::optional<std::string>& to) {
    if (!from.has_value() && !to.has_value()) {
        return std::nullopt;
    }
    std::string tz = timeZone();
    int64_t start = from.has_value() ? unixMsToChromeMicros(local_day_start_ms(*from, tz)) : 0LL;
    int64_t end = to.has_value() ? unixMsToChromeMicros(local_day_start_ms(nextDay(*to), tz)) : (1LL << 62);
    return ChromeRange{.start = start, .end = end};
}

template <typename T, typename RowMapper>
inline std::vector<T> acrossProfiles(
    const std::optional<std::string>& profileFilter,
    int limitCount,
    const std::string& sql,
    const std::vector<slimr::SqlValue>& params,
    RowMapper&& toRow
) {
    struct MergedItem {
        int64_t chrome;
        T row;
    };
    std::vector<MergedItem> merged;

    for (const auto& profile : selectProfiles(profileFilter)) {
        std::string cachedPath = ensureCachedHistory(profile);
        slimr::SqliteDb db(cachedPath, slimr::SqliteOptions{.readOnly = true});
        auto rows = db.all<slimr::Json>(sql, params);
        for (const auto& row : rows) {
            int64_t chrome = 0;
            if (row.contains("lastChrome") && !row["lastChrome"].is_null()) {
                chrome = row["lastChrome"].get<int64_t>();
            } else if (row.contains("timeChrome") && !row["timeChrome"].is_null()) {
                chrome = row["timeChrome"].get<int64_t>();
            }
            merged.push_back(MergedItem{
                .chrome = chrome,
                .row = toRow(row, profile),
            });
        }
    }

    std::stable_sort(merged.begin(), merged.end(), [](const MergedItem& a, const MergedItem& b) {
        return a.chrome > b.chrome;
    });

    size_t limit = limitCount > 0 ? static_cast<size_t>(limitCount) : 0;
    if (limit > 0 && merged.size() > limit) {
        merged.resize(limit);
    }

    std::vector<T> result;
    result.reserve(merged.size());
    for (auto& item : merged) {
        result.push_back(std::move(item.row));
    }
    return result;
}

inline std::vector<PageRow> queryPages(const PageQuery& query) {
    auto range = chromeRange(query.from, query.to);
    std::vector<std::string> where = {"u.hidden = 0"};
    std::vector<slimr::SqlValue> params;
    std::vector<slimr::SqlValue> joinParams;
    std::string joinSql = "LEFT JOIN visits v ON v.url = u.id";
    std::string having = "lastChrome > 0";

    if (range) {
        joinSql += " AND v.visit_time >= ? AND v.visit_time < ?";
        joinParams.push_back(range->start);
        joinParams.push_back(range->end);
        having = "MAX(v.visit_time) IS NOT NULL OR (u.last_visit_time >= ? AND u.last_visit_time < ?)";
    }

    for (const auto& word : query.words) {
        where.push_back("(u.title LIKE ? ESCAPE '\\' OR u.url LIKE ? ESCAPE '\\')");
        std::string pattern = "%" + escapeLike(word) + "%";
        params.push_back(pattern);
        params.push_back(pattern);
    }

    auto domain = normalizeDomain(query.domain);
    if (domain) {
        std::string d = escapeLike(*domain);
        where.push_back("(" + HOST_SQL + " = ? OR " +
                        HOST_SQL + " LIKE ? ESCAPE '\\' OR " +
                        HOST_SQL + " LIKE ? ESCAPE '\\' OR " +
                        HOST_SQL + " LIKE ? ESCAPE '\\')");
        params.push_back(*domain);
        params.push_back("%." + d);
        params.push_back(d + ":%");
        params.push_back("%." + d + ":%");
    }

    std::string whereClause;
    for (size_t i = 0; i < where.size(); ++i) {
        if (i > 0) whereClause += " AND ";
        whereClause += where[i];
    }

    std::string sql = "SELECT u.url, u.title, u.visit_count AS visitCount, "
                      "COALESCE(MAX(v.visit_time), u.last_visit_time) AS lastChrome, "
                      "(COALESCE(MAX(v.visit_time), u.last_visit_time) - " + std::to_string(CHROME_EPOCH_OFFSET_MICROS) + ") / 1000 AS lastMs "
                      "FROM urls u " + joinSql + " "
                      "WHERE " + whereClause + " "
                      "GROUP BY u.id "
                      "HAVING " + having + " "
                      "ORDER BY lastChrome DESC "
                      "LIMIT ?";

    std::vector<slimr::SqlValue> allParams;
    for (const auto& p : joinParams) allParams.push_back(p);
    for (const auto& p : params) allParams.push_back(p);
    if (range) {
        allParams.push_back(range->start);
        allParams.push_back(range->end);
    }
    allParams.push_back(static_cast<int64_t>(query.limit));

    std::string tz = timeZone();
    return acrossProfiles<PageRow>(query.profile, query.limit, sql, allParams, [&](const slimr::Json& row, const BraveProfile& profile) {
        std::string title;
        if (row.contains("title") && !row["title"].is_null()) {
            title = row["title"].get<std::string>();
        }
        std::string url = row["url"].get<std::string>();
        int64_t lastMs = row["lastMs"].get<int64_t>();
        int64_t visitCount = row["visitCount"].get<int64_t>();
        return PageRow{
            .title = title,
            .url = url,
            .lastVisit = format_local_iso(lastMs, tz),
            .visitCount = visitCount,
            .profile = profile.dir,
        };
    });
}

inline std::vector<VisitRow> queryVisits(const VisitQuery& query) {
    auto range = chromeRange(query.from, query.to);
    std::vector<std::string> where;
    std::vector<slimr::SqlValue> params;

    if (query.prefix) {
        where.push_back("u.url LIKE ? ESCAPE '\\'");
        params.push_back(escapeLike(query.url) + "%");
    } else {
        where.push_back("(u.url = ? OR u.url = ?)");
        params.push_back(query.url);
        params.push_back(query.url + "/");
    }

    if (range) {
        where.push_back("v.visit_time >= ? AND v.visit_time < ?");
        params.push_back(range->start);
        params.push_back(range->end);
    }

    std::string whereClause;
    for (size_t i = 0; i < where.size(); ++i) {
        if (i > 0) whereClause += " AND ";
        whereClause += where[i];
    }

    std::string sql = "SELECT u.url, u.title, v.visit_time AS timeChrome, "
                      "(v.visit_time - " + std::to_string(CHROME_EPOCH_OFFSET_MICROS) + ") / 1000 AS timeMs, "
                      "v.transition & 255 AS coreTransition, v.visit_duration AS durationMicros "
                      "FROM visits v JOIN urls u ON u.id = v.url "
                      "WHERE " + whereClause + " "
                      "ORDER BY v.visit_time DESC "
                      "LIMIT ?";

    std::vector<slimr::SqlValue> allParams = params;
    allParams.push_back(static_cast<int64_t>(query.limit));

    std::string tz = timeZone();
    return acrossProfiles<VisitRow>(query.profile, query.limit, sql, allParams, [&](const slimr::Json& row, const BraveProfile& profile) {
        std::string title;
        if (row.contains("title") && !row["title"].is_null()) {
            title = row["title"].get<std::string>();
        }
        std::string url = row["url"].get<std::string>();
        int64_t timeMs = row["timeMs"].get<int64_t>();
        int64_t coreTrans = row["coreTransition"].get<int64_t>();
        int64_t durationMicros = 0;
        if (row.contains("durationMicros") && !row["durationMicros"].is_null()) {
            durationMicros = row["durationMicros"].get<int64_t>();
        }
        std::string transition = (coreTrans >= 0 && static_cast<size_t>(coreTrans) < TRANSITIONS.size())
            ? TRANSITIONS[static_cast<size_t>(coreTrans)]
            : ("unknown_" + std::to_string(coreTrans));
        double durationSeconds = std::round(static_cast<double>(durationMicros) / 100000.0) / 10.0;
        return VisitRow{
            .time = format_local_iso(timeMs, tz),
            .url = url,
            .title = title,
            .transition = transition,
            .durationSeconds = durationSeconds,
            .profile = profile.dir,
        };
    });
}

} // namespace brave_history
