#pragma once

#include "snapshot.hpp"
#include "time.hpp"
#include "slimr/sqlite.hpp"
#include "slimr/test.hpp"

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace brave_history::testing {

inline int64_t parse_iso_utc_ms(const std::string& s) {
    struct tm tm_utc{};
    int y = std::stoi(s.substr(0, 4));
    int m = std::stoi(s.substr(5, 2));
    int d = std::stoi(s.substr(8, 2));
    int h = std::stoi(s.substr(11, 2));
    int min = std::stoi(s.substr(14, 2));
    int sec = std::stoi(s.substr(17, 2));
    tm_utc.tm_year = y - 1900;
    tm_utc.tm_mon = m - 1;
    tm_utc.tm_mday = d;
    tm_utc.tm_hour = h;
    tm_utc.tm_min = min;
    tm_utc.tm_sec = sec;
    time_t t = timegm(&tm_utc);
    int64_t ms = static_cast<int64_t>(t) * 1000LL;
    size_t dot = s.find('.', 17);
    if (dot != std::string::npos) {
        int millis = std::stoi(s.substr(dot + 1, 3));
        ms += millis;
    }
    return ms;
}

inline void setMtime(const std::string& path, int64_t ms) {
    struct timeval tv[2];
    tv[0].tv_sec = static_cast<time_t>(ms / 1000);
    tv[0].tv_usec = static_cast<suseconds_t>((ms % 1000) * 1000);
    tv[1] = tv[0];
    utimes(path.c_str(), tv);
}

struct FixtureVisit {
    std::string at;
    int64_t durationMicros = 0;
    int64_t transition = 0;
};

struct FixturePage {
    std::string url;
    std::string title;
    bool hidden = false;
    std::vector<FixtureVisit> visits;
};

inline void addPage(slimr::SqliteDb& db, const FixturePage& page) {
    std::vector<int64_t> times;
    times.reserve(page.visits.size());
    for (const auto& v : page.visits) {
        times.push_back(unixMsToChromeMicros(parse_iso_utc_ms(v.at)));
    }
    int64_t last = times.empty() ? 0LL : *std::max_element(times.begin(), times.end());

    auto res = db.run(
        "INSERT INTO urls (url, title, visit_count, last_visit_time, hidden) VALUES (?, ?, ?, ?, ?)",
        {page.url, page.title, static_cast<int64_t>(page.visits.size()), last, page.hidden ? 1LL : 0LL}
    );
    int64_t urlId = res.lastInsertRowid;

    for (size_t i = 0; i < page.visits.size(); ++i) {
        db.run(
            "INSERT INTO visits (url, visit_time, transition, visit_duration) VALUES (?, ?, ?, ?)",
            {urlId, times[i], page.visits[i].transition, page.visits[i].durationMicros}
        );
    }
}

struct Fixture {
    std::filesystem::path base;
    std::string root;
    std::string cache;

    Fixture() = default;
    Fixture(std::filesystem::path b)
        : base(std::move(b)),
          root((base / "Brave-Browser").string()),
          cache((base / "cache").string()) {}

    ~Fixture() {
        std::error_code ec;
        if (!base.empty()) {
            std::filesystem::remove_all(base, ec);
        }
    }

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    Fixture(Fixture&& o) noexcept
        : base(std::move(o.base)), root(std::move(o.root)), cache(std::move(o.cache)) {
        o.base.clear();
    }

    Fixture& operator=(Fixture&& o) noexcept {
        if (this != &o) {
            std::error_code ec;
            if (!base.empty()) {
                std::filesystem::remove_all(base, ec);
            }
            base = std::move(o.base);
            root = std::move(o.root);
            cache = std::move(o.cache);
            o.base.clear();
        }
        return *this;
    }
};

inline std::string historyPath(const Fixture& fixture, const std::string& profileDir) {
    return (std::filesystem::path(fixture.root) / profileDir / "History").string();
}

inline Fixture createFixture(
    const std::map<std::string, std::vector<FixturePage>>& profiles,
    const std::map<std::string, std::string>& names = {}
) {
    auto base = slimr::testing::temp_dir("brave_history_test_");
    Fixture fixture(base);

    std::filesystem::create_directories(fixture.root);
    std::filesystem::create_directories(fixture.cache);

    slimr::Json localState = slimr::Json::object_t{};
    slimr::Json infoCache = slimr::Json::object_t{};
    for (const auto& [dir, name] : names) {
        infoCache[dir]["name"] = name;
    }
    localState["profile"]["info_cache"] = infoCache;
    {
        std::ofstream f(std::filesystem::path(fixture.root) / "Local State");
        f << slimr::write_json(localState);
    }

    for (const auto& [dir, pages] : profiles) {
        auto profDir = std::filesystem::path(fixture.root) / dir;
        std::filesystem::create_directories(profDir);
        std::string dbPath = historyPath(fixture, dir);
        slimr::SqliteDb db(dbPath, slimr::SqliteOptions{.readOnly = false, .enableDefaultPragmas = false});
        db.exec(
            "CREATE TABLE urls (id INTEGER PRIMARY KEY AUTOINCREMENT, url LONGVARCHAR, title LONGVARCHAR, "
            "visit_count INTEGER DEFAULT 0 NOT NULL, typed_count INTEGER DEFAULT 0 NOT NULL, "
            "last_visit_time INTEGER NOT NULL, hidden INTEGER DEFAULT 0 NOT NULL); "
            "CREATE TABLE visits (id INTEGER PRIMARY KEY, url INTEGER NOT NULL, visit_time INTEGER NOT NULL, "
            "from_visit INTEGER, transition INTEGER DEFAULT 0 NOT NULL, segment_id INTEGER, "
            "visit_duration INTEGER DEFAULT 0 NOT NULL);"
        );
        for (const auto& page : pages) {
            addPage(db, page);
        }
        db.close();
    }

    setenv("BRAVE_HISTORY_ROOT", fixture.root.c_str(), 1);
    setenv("BRAVE_HISTORY_CACHE", fixture.cache.c_str(), 1);
    setenv("BRAVE_HISTORY_TZ", "Europe/Dublin", 1);

    return fixture;
}

} // namespace brave_history::testing
