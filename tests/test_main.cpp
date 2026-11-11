#define SLIMR_TEST_MAIN
#include "test.hpp"

#include "app.hpp"
#include "fixture.hpp"
#include "mcp_http.hpp"
#include "schemas.hpp"
#include "snapshot.hpp"
#include "time.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace brave_history;
using namespace brave_history::testing;

// ============================================================================
// 1. Chromium timestamps
// ============================================================================
TEST_SUITE("chromium timestamps") {
    TEST_CASE("epoch offset: 1601-01-01 and 1970-01-01") {
        CHECK(chromeMicrosToUnixMs(11644473600000000LL) == 0);
        CHECK(chromeMicrosToUnixMs(0LL) == -11644473600000LL);
    }

    TEST_CASE("round-trips a real instant beyond Number.MAX_SAFE_INTEGER micros") {
        int64_t ms = parse_iso_utc_ms("2026-11-07T23:23:26.123Z");
        int64_t chrome = unixMsToChromeMicros(ms);
        CHECK(chrome > 9007199254740991LL);
        CHECK(chrome == 13438567406123000LL);
        CHECK(chromeMicrosToUnixMs(chrome) == ms);
    }
}

// ============================================================================
// 2. Europe/Dublin local time
// ============================================================================
TEST_SUITE("Europe/Dublin local time") {
    TEST_CASE("winter is +00:00, summer is +01:00") {
        CHECK(format_local_iso(parse_iso_utc_ms("2026-01-15T09:30:05Z"), "Europe/Dublin") == "2026-01-15T09:30:05+00:00");
        CHECK(format_local_iso(parse_iso_utc_ms("2026-07-01T12:00:00Z"), "Europe/Dublin") == "2026-07-01T13:00:00+01:00");
        CHECK(format_local_iso(parse_iso_utc_ms("2026-07-01T12:00:00Z"), "America/New_York") == "2026-07-01T08:00:00-04:00");
    }

    TEST_CASE("local day start honours the offset") {
        CHECK(local_day_start_ms("2026-01-15", "Europe/Dublin") == parse_iso_utc_ms("2026-01-15T00:00:00Z"));
        CHECK(local_day_start_ms("2026-07-01", "Europe/Dublin") == parse_iso_utc_ms("2026-06-30T23:00:00Z"));
        CHECK_THROWS_WITH(local_day_start_ms("2026-7-1", "Europe/Dublin"), doctest::Contains("YYYY-MM-DD"));
    }
}

// ============================================================================
// 3. Status command
// ============================================================================
TEST_SUITE("status command") {
    TEST_CASE("exports outputSchema and json option") {
        auto app = make_brave_history_app();
        CHECK(app.tools().contains("status"));
        const auto& statusTool = app.tools().at("status");
        CHECK(statusTool.inputSchema.contains("properties"));
        CHECK(statusTool.inputSchema["properties"].contains("json"));
    }
}

// ============================================================================
// 4. Cached History copy
// ============================================================================
TEST_SUITE("cached History copy") {
    auto page = [](const std::string& url, const std::string& at) {
        return FixturePage{.url = url, .title = url, .visits = {{.at = at}}};
    };

    auto urls_of = []() {
        auto pages = queryPages(PageQuery{.words = {}, .limit = 50});
        std::vector<std::string> res;
        res.reserve(pages.size());
        for (const auto& p : pages) res.push_back(p.url);
        return res;
    };

    TEST_CASE("discovers Default and Profile N, with display names") {
        auto fixture = createFixture(
            {
                {"Default", {page("https://a.example/", "2026-01-01T10:00:00Z")}},
                {"Profile 2", {}},
                {"Junk", {}},
            },
            {
                {"Default", "Personal"},
                {"Profile 2", "Work"},
            }
        );
        std::string source = historyPath(fixture, "Default");
        setMtime(source, parse_iso_utc_ms("2026-01-01T10:00:00Z"));

        auto profiles = listProfiles();
        CHECK(profiles.size() == 2);
        CHECK(profiles[0].dir == "Default");
        CHECK(profiles[0].name.has_value());
        CHECK(*profiles[0].name == "Personal");
        CHECK(profiles[1].dir == "Profile 2");
        CHECK(profiles[1].name.has_value());
        CHECK(*profiles[1].name == "Work");

        auto work = selectProfiles("work");
        CHECK(work.size() == 1);
        CHECK(work[0].dir == "Profile 2");

        CHECK_THROWS_WITH(selectProfiles("nope"), doctest::Contains("No Brave profile matches \"nope\""));
    }

    TEST_CASE("copies on first use and stamps the copy with the original's mtime") {
        auto fixture = createFixture(
            {
                {"Default", {page("https://a.example/", "2026-01-01T10:00:00Z")}},
                {"Profile 2", {}},
                {"Junk", {}},
            },
            {
                {"Default", "Personal"},
                {"Profile 2", "Work"},
            }
        );
        std::string source = historyPath(fixture, "Default");
        setMtime(source, parse_iso_utc_ms("2026-01-01T10:00:00Z"));

        std::string cached = ensureCachedHistory(BraveProfile{.dir = "Default"});
        CHECK(cached == (std::filesystem::path(fixture.cache) / "Default" / "History").string());
        CHECK(get_file_mtime_ms(cached) == get_file_mtime_ms(source));
        CHECK(urls_of() == std::vector<std::string>{"https://a.example/"});
    }

    TEST_CASE("skips the copy while the original is not newer") {
        auto fixture = createFixture(
            {
                {"Default", {page("https://a.example/", "2026-01-01T10:00:00Z")}},
                {"Profile 2", {}},
            },
            {{"Default", "Personal"}}
        );
        std::string source = historyPath(fixture, "Default");
        setMtime(source, parse_iso_utc_ms("2026-01-01T10:00:00Z"));
        ensureCachedHistory(BraveProfile{.dir = "Default"});

        {
            slimr::SqliteDb db(source, slimr::SqliteOptions{.readOnly = false, .enableDefaultPragmas = false});
            addPage(db, page("https://b.example/", "2026-01-02T10:00:00Z"));
        }
        setMtime(source, parse_iso_utc_ms("2026-01-01T10:00:00Z"));
        CHECK(urls_of() == std::vector<std::string>{"https://a.example/"});
    }

    TEST_CASE("re-copies when the original is newer") {
        auto fixture = createFixture(
            {
                {"Default", {page("https://a.example/", "2026-01-01T10:00:00Z")}},
            },
            {{"Default", "Personal"}}
        );
        std::string source = historyPath(fixture, "Default");
        setMtime(source, parse_iso_utc_ms("2026-01-01T10:00:00Z"));
        ensureCachedHistory(BraveProfile{.dir = "Default"});

        {
            slimr::SqliteDb db(source, slimr::SqliteOptions{.readOnly = false, .enableDefaultPragmas = false});
            addPage(db, page("https://b.example/", "2026-01-02T10:00:00Z"));
        }
        setMtime(source, parse_iso_utc_ms("2026-01-03T10:00:00Z"));
        CHECK(urls_of() == std::vector<std::string>{"https://b.example/", "https://a.example/"});
    }

    TEST_CASE("re-copies when only the -journal sidecar is newer") {
        auto fixture = createFixture(
            {
                {"Default", {page("https://a.example/", "2026-01-01T10:00:00Z"), page("https://b.example/", "2026-01-02T10:00:00Z")}},
            },
            {{"Default", "Personal"}}
        );
        std::string source = historyPath(fixture, "Default");
        setMtime(source, parse_iso_utc_ms("2026-01-03T10:00:00Z"));
        ensureCachedHistory(BraveProfile{.dir = "Default"});

        {
            slimr::SqliteDb db(source, slimr::SqliteOptions{.readOnly = false, .enableDefaultPragmas = false});
            addPage(db, page("https://c.example/", "2026-01-04T10:00:00Z"));
        }
        setMtime(source, parse_iso_utc_ms("2026-01-03T10:00:00Z"));
        std::string journal = source + "-journal";
        {
            slimr::SqliteDb memDb(":memory:");
            memDb.exec("VACUUM INTO '" + journal + "';");
        }
        setMtime(journal, parse_iso_utc_ms("2026-01-05T10:00:00Z"));
        CHECK(urls_of() == std::vector<std::string>{"https://c.example/", "https://b.example/", "https://a.example/"});
        std::string cached = (std::filesystem::path(fixture.cache) / "Default" / "History").string();
        CHECK(get_file_mtime_ms(cached) == parse_iso_utc_ms("2026-01-05T10:00:00Z"));
    }

    TEST_CASE("never writes to the Brave data dir") {
        auto fixture = createFixture(
            {{"Default", {page("https://a.example/", "2026-01-01T10:00:00Z")}}},
            {{"Default", "Personal"}}
        );
        std::string source = historyPath(fixture, "Default");
        std::vector<std::string> before_files;
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(fixture.root) / "Default")) {
            before_files.push_back(entry.path().filename().string());
        }
        std::sort(before_files.begin(), before_files.end());

        int64_t mtime = get_file_mtime_ms(source);
        setMtime(source, mtime + 60000);
        urls_of();

        std::vector<std::string> after_files;
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(fixture.root) / "Default")) {
            after_files.push_back(entry.path().filename().string());
        }
        std::sort(after_files.begin(), after_files.end());

        CHECK(after_files == before_files);
        CHECK(get_file_mtime_ms(source) == mtime + 60000);
        CHECK(!std::filesystem::exists(source + "-wal"));
    }

    TEST_CASE("refuses a cache dir inside the Brave data dir") {
        auto fixture = createFixture(
            {{"Default", {page("https://a.example/", "2026-01-01T10:00:00Z")}}},
            {{"Default", "Personal"}}
        );
        const char* saved = std::getenv("BRAVE_HISTORY_CACHE");
        std::string savedStr = saved ? saved : "";
        std::string badCache = (std::filesystem::path(fixture.root) / "cache").string();
        setenv("BRAVE_HISTORY_CACHE", badCache.c_str(), 1);
        CHECK_THROWS_WITH(ensureCachedHistory(BraveProfile{.dir = "Default"}), doctest::Contains("Refusing to cache"));
        if (!savedStr.empty()) {
            setenv("BRAVE_HISTORY_CACHE", savedStr.c_str(), 1);
        }
    }

    TEST_CASE("cache dir is 0o700 and the copied files 0o600, including pre-existing loose ones") {
        auto fixture = createFixture(
            {{"Default", {page("https://a.example/", "2026-01-01T10:00:00Z")}}},
            {{"Default", "Personal"}}
        );
        std::string source = historyPath(fixture, "Default");
        auto mode_of = [](const std::string& p) {
            struct stat st;
            stat(p.c_str(), &st);
            return st.st_mode & 0777;
        };
        std::string dir = (std::filesystem::path(fixture.cache) / "Default").string();
        setMtime(source, parse_iso_utc_ms("2026-01-06T10:00:00Z"));
        ensureCachedHistory(BraveProfile{.dir = "Default"});

        CHECK(mode_of(fixture.cache) == 0700);
        CHECK(mode_of(dir) == 0700);
        CHECK(mode_of(dir + "/History") == 0600);

        chmod(dir.c_str(), 0755);
        chmod((dir + "/History").c_str(), 0644);
        ensureCachedHistory(BraveProfile{.dir = "Default"}); // up to date: no copy, still tightened
        CHECK(mode_of(dir) == 0700);
        CHECK(mode_of(dir + "/History") == 0600);
    }

    TEST_CASE("staging uses mkdtemp and leaves no temp dirs behind") {
        auto fixture = createFixture(
            {{"Default", {page("https://a.example/", "2026-01-01T10:00:00Z")}}},
            {{"Default", "Personal"}}
        );
        std::string source = historyPath(fixture, "Default");
        setMtime(source, parse_iso_utc_ms("2026-01-07T10:00:00Z"));
        ensureCachedHistory(BraveProfile{.dir = "Default"});

        std::vector<std::string> tmpDirs;
        for (const auto& entry : std::filesystem::directory_iterator(fixture.cache)) {
            if (entry.path().filename().string().find(".tmp-") != std::string::npos) {
                tmpDirs.push_back(entry.path().filename().string());
            }
        }
        CHECK(tmpDirs.empty());
    }
}

// ============================================================================
// 5. queryPages (search/list) & queryVisits
// ============================================================================
TEST_SUITE("queryPages and queryVisits") {
    constexpr int64_t SERVER_REDIRECT = 0x80000000LL;
    constexpr int64_t CHAIN_START = 0x10000000LL;

    auto setup_fixture = []() {
        return createFixture({
            {"Default", {
                {
                    .url = "https://www.amazon.com/Standing-Desk/dp/B01",
                    .title = "FlexiSpot Standing Desk",
                    .visits = {
                        {.at = "2026-06-30T22:30:00Z", .transition = 1 | CHAIN_START},
                        {.at = "2026-07-02T10:00:00Z", .durationMicros = 95000000, .transition = 0},
                    },
                },
                {.url = "https://amazon.com/", .title = "Amazon", .visits = {{.at = "2026-07-01T09:00:00Z"}}},
                {.url = "https://notamazon.com/desk", .title = "Desk deals", .visits = {{.at = "2026-07-01T09:30:00Z"}}},
                {.url = "https://smile.amazon.com:8443/x", .title = "Smile", .visits = {{.at = "2025-12-31T23:59:00Z"}}},
                {
                    .url = "https://example.com/100%25-off",
                    .title = "100% off sale",
                    .visits = {{.at = "2026-01-02T00:00:00Z", .transition = 8 | SERVER_REDIRECT}},
                },
                {
                    .url = "https://example.com/hidden",
                    .title = "Hidden desk frame",
                    .hidden = true,
                    .visits = {{.at = "2026-07-03T00:00:00Z"}},
                },
            }},
            {"Profile 1", {
                {
                    .url = "https://news.example.org/standing-desks",
                    .title = "Best standing desks",
                    .visits = {{.at = "2026-07-01T12:00:00Z"}},
                },
            }},
        });
    };

    auto search = [](const std::vector<std::string>& words,
                     const std::optional<std::string>& domain = std::nullopt,
                     const std::optional<std::string>& from = std::nullopt,
                     const std::optional<std::string>& to = std::nullopt,
                     const std::optional<std::string>& profile = std::nullopt,
                     int limit = 25) {
        PageQuery q;
        q.words = words;
        q.domain = domain;
        q.from = from;
        q.to = to;
        q.profile = profile;
        q.limit = limit;
        auto pages = queryPages(q);
        std::vector<std::string> urls;
        for (const auto& p : pages) urls.push_back(p.url);
        return urls;
    };

    TEST_CASE("ANDs words across title and URL, case-insensitive, across profiles newest first") {
        auto fixture = setup_fixture();
        CHECK(search({"STANDING", "desk"}) == std::vector<std::string>{
            "https://www.amazon.com/Standing-Desk/dp/B01",
            "https://news.example.org/standing-desks",
        });
        CHECK(search({"flexispot", "amazon"}) == std::vector<std::string>{
            "https://www.amazon.com/Standing-Desk/dp/B01",
        });
        CHECK(search({"flexispot", "ebay"}).empty());
    }

    TEST_CASE("treats % and _ literally") {
        auto fixture = setup_fixture();
        CHECK(search({"100%"}) == std::vector<std::string>{"https://example.com/100%25-off"});
        CHECK(search({"_"}).empty());
    }

    TEST_CASE("excludes hidden URLs") {
        auto fixture = setup_fixture();
        CHECK(search({"frame"}).empty());
    }

    TEST_CASE("domain matches the host and subdomains (with ports), not lookalikes") {
        auto fixture = setup_fixture();
        CHECK(search({}, "amazon.com") == std::vector<std::string>{
            "https://www.amazon.com/Standing-Desk/dp/B01",
            "https://amazon.com/",
            "https://smile.amazon.com:8443/x",
        });
        CHECK(search({}, "https://WWW.Amazon.com/whatever") == std::vector<std::string>{
            "https://www.amazon.com/Standing-Desk/dp/B01",
        });
    }

    TEST_CASE("date range is inclusive local days and reports the last visit inside it") {
        auto fixture = setup_fixture();
        PageQuery q;
        q.words = {"desk"};
        q.from = "2026-06-30";
        q.to = "2026-06-30";
        q.limit = 25;
        auto pages = queryPages(q);
        CHECK(pages.size() == 1);
        CHECK(pages[0].url == "https://www.amazon.com/Standing-Desk/dp/B01");
        CHECK(pages[0].lastVisit == "2026-06-30T23:30:00+01:00");
        CHECK(pages[0].visitCount == 2);

        CHECK(search({}, std::nullopt, "2026-07-01", "2026-07-01") == std::vector<std::string>{
            "https://news.example.org/standing-desks",
            "https://notamazon.com/desk",
            "https://amazon.com/",
        });
        CHECK(search({}, std::nullopt, std::nullopt, "2025-12-31") == std::vector<std::string>{
            "https://smile.amazon.com:8443/x",
        });
        CHECK(search({"desk"}, std::nullopt, "2026-07-02") == std::vector<std::string>{
            "https://www.amazon.com/Standing-Desk/dp/B01",
        });
    }

    TEST_CASE("profile filter and limit") {
        auto fixture = setup_fixture();
        CHECK(search({"desk"}, std::nullopt, std::nullopt, std::nullopt, "Profile 1") == std::vector<std::string>{
            "https://news.example.org/standing-desks",
        });
        CHECK(search({}, std::nullopt, std::nullopt, std::nullopt, std::nullopt, 2).size() == 2);
    }

    TEST_CASE("returns title, ISO Dublin last visit, visit count and profile") {
        auto fixture = setup_fixture();
        PageQuery q;
        q.words = {"flexispot"};
        q.limit = 1;
        auto pages = queryPages(q);
        CHECK(pages.size() == 1);
        CHECK(pages[0].title == "FlexiSpot Standing Desk");
        CHECK(pages[0].url == "https://www.amazon.com/Standing-Desk/dp/B01");
        CHECK(pages[0].lastVisit == "2026-07-02T11:00:00+01:00");
        CHECK(pages[0].visitCount == 2);
        CHECK(pages[0].profile == "Default");
    }

    TEST_CASE("exact URL returns every visit with transition and duration, newest first") {
        auto fixture = setup_fixture();
        VisitQuery q;
        q.url = "https://www.amazon.com/Standing-Desk/dp/B01";
        q.prefix = false;
        q.limit = 25;
        auto visits = queryVisits(q);
        CHECK(visits.size() == 2);
        CHECK(visits[0].time == "2026-07-02T11:00:00+01:00");
        CHECK(visits[0].url == "https://www.amazon.com/Standing-Desk/dp/B01");
        CHECK(visits[0].title == "FlexiSpot Standing Desk");
        CHECK(visits[0].transition == "link");
        CHECK(visits[0].durationSeconds == 95.0);
        CHECK(visits[0].profile == "Default");

        CHECK(visits[1].time == "2026-06-30T23:30:00+01:00");
        CHECK(visits[1].url == "https://www.amazon.com/Standing-Desk/dp/B01");
        CHECK(visits[1].title == "FlexiSpot Standing Desk");
        CHECK(visits[1].transition == "typed");
        CHECK(visits[1].durationSeconds == 0.0);
        CHECK(visits[1].profile == "Default");
    }

    TEST_CASE("exact URL also tries a trailing slash; qualifier bits are masked off") {
        auto fixture = setup_fixture();
        VisitQuery q1;
        q1.url = "https://amazon.com";
        q1.prefix = false;
        q1.limit = 25;
        auto v1 = queryVisits(q1);
        CHECK(v1.size() == 1);
        CHECK(v1[0].url == "https://amazon.com/");

        VisitQuery q2;
        q2.url = "https://example.com/100%25-off";
        q2.prefix = false;
        q2.limit = 25;
        auto v2 = queryVisits(q2);
        CHECK(v2.size() == 1);
        CHECK(v2[0].transition == "reload");
    }

    TEST_CASE("prefix matches URLs under it and honours the date range") {
        auto fixture = setup_fixture();
        VisitQuery q1;
        q1.url = "https://www.amazon.com/";
        q1.prefix = true;
        q1.limit = 25;
        auto v1 = queryVisits(q1);
        CHECK(v1.size() == 2);
        CHECK(v1[0].time == "2026-07-02T11:00:00+01:00");
        CHECK(v1[1].time == "2026-06-30T23:30:00+01:00");

        VisitQuery q2;
        q2.url = "https://www.amazon.com/";
        q2.prefix = true;
        q2.from = "2026-07-01";
        q2.limit = 25;
        auto v2 = queryVisits(q2);
        CHECK(v2.size() == 1);

        VisitQuery q3;
        q3.url = "https://example.com/1";
        q3.prefix = false;
        q3.limit = 25;
        auto v3 = queryVisits(q3);
        CHECK(v3.empty());
    }
}

namespace {

slimr::Json make_mcp_request(int id, std::string_view method, slimr::Json params = slimr::json_obj()) {
    slimr::Json meta = slimr::json_obj();
    meta["io.modelcontextprotocol/protocolVersion"] = std::string(slimr::kMcpProtocolVersion);
    meta["io.modelcontextprotocol/clientCapabilities"] = slimr::json_obj();
    params["_meta"] = std::move(meta);
    slimr::Json req = slimr::json_obj();
    req["jsonrpc"] = "2.0";
    req["id"] = id;
    req["method"] = std::string(method);
    req["params"] = std::move(params);
    return req;
}

slimr::Json make_tool_call(int id, std::string_view name, slimr::Json arguments) {
    slimr::Json params = slimr::json_obj();
    params["name"] = std::string(name);
    params["arguments"] = std::move(arguments);
    return make_mcp_request(id, "tools/call", std::move(params));
}

struct TcpFd {
    int fd = -1;
    TcpFd() = default;
    explicit TcpFd(int f) : fd(f) {}
    ~TcpFd() {
        if (fd >= 0) ::close(fd);
    }
    TcpFd(const TcpFd&) = delete;
    TcpFd& operator=(const TcpFd&) = delete;
};

struct HttpResponse {
    int status = 0;
    std::string body;
};

HttpResponse http_post_mcp(uint16_t port, std::string_view method, std::string_view mcpName, const slimr::Json& req) {
    TcpFd sock(::socket(AF_INET, SOCK_STREAM, 0));
    if (sock.fd < 0) return {};

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) return {};
    if (::connect(sock.fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) return {};

    const std::string bodyStr = slimr::to_json(req);
    std::ostringstream os;
    os << "POST /mcp HTTP/1.1\r\n"
       << "Host: 127.0.0.1:" << port << "\r\n"
       << "Connection: close\r\n"
       << "Content-Type: application/json\r\n"
       << "Accept: application/json, text/event-stream\r\n"
       << "MCP-Protocol-Version: " << slimr::kMcpProtocolVersion << "\r\n"
       << "Mcp-Method: " << method << "\r\n";
    if (!mcpName.empty()) {
        os << "Mcp-Name: " << mcpName << "\r\n";
    }
    os << "Content-Length: " << bodyStr.size() << "\r\n\r\n" << bodyStr;
    const std::string rawReq = os.str();

    size_t sent = 0;
    while (sent < rawReq.size()) {
        const ssize_t n = ::send(sock.fd, rawReq.data() + sent, rawReq.size() - sent, 0);
        if (n <= 0) return {};
        sent += static_cast<size_t>(n);
    }

    std::string rawResp;
    char buf[4096]{};
    for (;;) {
        const ssize_t n = ::recv(sock.fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        rawResp.append(buf, static_cast<size_t>(n));
    }
    const auto hdrEnd = rawResp.find("\r\n\r\n");
    if (hdrEnd == std::string::npos) return {};
    const std::string hdrs = rawResp.substr(0, hdrEnd);
    const auto sp1 = hdrs.find(' ');
    const int status = sp1 == std::string::npos ? 0 : static_cast<int>(std::strtol(hdrs.c_str() + sp1 + 1, nullptr, 10));
    return {status, rawResp.substr(hdrEnd + 4)};
}

}  // namespace

// ============================================================================
// 6. App level & URL stripping tests
// ============================================================================
TEST_SUITE("app and URL stripping") {
    TEST_CASE("exposes the history commands") {
        auto app = make_brave_history_app();
        std::vector<std::string> tools;
        for (const auto& [name, _] : app.tools()) {
            tools.push_back(name);
        }
        std::sort(tools.begin(), tools.end());
        CHECK(tools == std::vector<std::string>{"list", "search", "status", "visits"});
    }

    TEST_CASE("search runs end to end on the MCP path") {
        auto fixture = createFixture({
            {"Default", {
                {.url = "https://example.com/desk", .title = "Standing desk", .visits = {{.at = "2026-03-01T10:00:00Z"}}},
            }},
        });
        auto app = make_brave_history_app();

        const slimr::Json req = make_tool_call(100, "search", slimr::Json::object_t{
            {"words", slimr::Json::array_t{"standing", "desk"}},
        });

        slimr::Json resp = app.dispatchMcpJson(req);
        CHECK(resp["result"]["isError"].get<bool>() == false);
        auto contents = resp["result"]["content"].get<slimr::Json::array_t>();
        CHECK(contents.size() == 1);
        auto parsed = slimr::parse_json<slimr::Json>(contents[0]["text"].get<std::string>());
        CHECK(parsed.has_value());
        CHECK((*parsed).contains("pages"));
        auto pages = (*parsed)["pages"].get<slimr::Json::array_t>();
        CHECK(pages.size() == 1);
        CHECK(pages[0]["title"].get<std::string>() == "Standing desk");
        CHECK(pages[0]["url"].get<std::string>() == "https://example.com/desk");
        CHECK(pages[0]["lastVisit"].get<std::string>() == "2026-03-01T10:00:00+00:00");
        CHECK(pages[0]["visitCount"].get<int64_t>() == 1);
        CHECK(pages[0]["profile"].get<std::string>() == "Default");
    }

    TEST_CASE("search, list and visits drop query and fragment by default") {
        const std::string FULL = "https://example.com/desk?token=abc&id=1#frag";
        auto fixture = createFixture({
            {"Default", {
                {.url = FULL, .title = "Desk", .visits = {{.at = "2026-03-01T10:00:00Z"}}},
            }},
        });
        auto app = make_brave_history_app();

        // 1. search
        {
            const auto req = make_tool_call(101, "search", slimr::Json::object_t{{"words", slimr::Json::array_t{"desk"}}});
            auto resp = app.dispatchMcpJson(req);
            auto parsed = slimr::parse_json<slimr::Json>(resp["result"]["content"][0]["text"].get<std::string>());
            CHECK((*parsed)["pages"][0]["url"].get<std::string>() == "https://example.com/desk");
        }

        // 2. list
        {
            const auto req = make_tool_call(102, "list", slimr::Json::object_t{});
            auto resp = app.dispatchMcpJson(req);
            auto parsed = slimr::parse_json<slimr::Json>(resp["result"]["content"][0]["text"].get<std::string>());
            CHECK((*parsed)["pages"][0]["url"].get<std::string>() == "https://example.com/desk");
        }

        // 3. visits
        {
            const auto req = make_tool_call(103, "visits", slimr::Json::object_t{{"url", FULL}});
            auto resp = app.dispatchMcpJson(req);
            auto parsed = slimr::parse_json<slimr::Json>(resp["result"]["content"][0]["text"].get<std::string>());
            CHECK((*parsed)["visits"][0]["url"].get<std::string>() == "https://example.com/desk");
        }
    }

    TEST_CASE("--fullUrls keeps the full URL") {
        const std::string FULL = "https://example.com/desk?token=abc&id=1#frag";
        auto fixture = createFixture({
            {"Default", {
                {.url = FULL, .title = "Desk", .visits = {{.at = "2026-03-01T10:00:00Z"}}},
            }},
        });
        auto app = make_brave_history_app();

        // 1. search with fullUrls: true
        {
            const auto req = make_tool_call(201, "search", slimr::Json::object_t{
                {"words", slimr::Json::array_t{"desk"}},
                {"fullUrls", true},
            });
            auto resp = app.dispatchMcpJson(req);
            auto parsed = slimr::parse_json<slimr::Json>(resp["result"]["content"][0]["text"].get<std::string>());
            CHECK((*parsed)["pages"][0]["url"].get<std::string>() == FULL);
        }

        // 2. list with fullUrls: true
        {
            const auto req = make_tool_call(202, "list", slimr::Json::object_t{
                {"fullUrls", true},
            });
            auto resp = app.dispatchMcpJson(req);
            auto parsed = slimr::parse_json<slimr::Json>(resp["result"]["content"][0]["text"].get<std::string>());
            CHECK((*parsed)["pages"][0]["url"].get<std::string>() == FULL);
        }

        // 3. visits with fullUrls: true
        {
            const auto req = make_tool_call(203, "visits", slimr::Json::object_t{
                {"url", FULL},
                {"fullUrls", true},
            });
            auto resp = app.dispatchMcpJson(req);
            auto parsed = slimr::parse_json<slimr::Json>(resp["result"]["content"][0]["text"].get<std::string>());
            CHECK((*parsed)["visits"][0]["url"].get<std::string>() == FULL);
        }
    }
}

// ============================================================================
// 7. MCP 2026-07-28, Streamable HTTP, CLI & Concurrency tests
// ============================================================================
TEST_SUITE("mcp 2026-07-28, http and concurrency") {
    TEST_CASE("dispatchMcpJson serves 2026-07-28 discovery and rejects legacy initialize") {
        auto app = make_brave_history_app();

        const auto initResp = app.dispatchMcpJson(make_mcp_request(1, "initialize"));
        CHECK(initResp["error"]["code"].get<int64_t>() == static_cast<int64_t>(slimr::McpErrorCode::MethodNotFound));

        const auto discResp = app.dispatchMcpJson(make_mcp_request(2, "server/discover"));
        CHECK(discResp["result"]["supportedVersions"][0].get<std::string>() == slimr::kMcpProtocolVersion);
        CHECK(discResp["result"]["_meta"]["io.modelcontextprotocol/serverInfo"]["name"].get<std::string>() == "brave-history-mcp");

        const auto listResp = app.dispatchMcpJson(make_mcp_request(3, "tools/list"));
        CHECK(listResp["result"]["tools"].get<slimr::Json::array_t>().size() == 4);
    }

    TEST_CASE("http security invariants and CLI behavior") {
        auto fixture = createFixture({
            {"Default", {
                {.url = "https://example.com/status-check", .title = "Status Page", .visits = {{.at = "2026-03-01T10:00:00Z"}}},
            }},
        });
        auto app = make_brave_history_app();

        slimr::HttpServerOptions nonLoopback;
        nonLoopback.host = "0.0.0.0";
        nonLoopback.port = 0;
        nonLoopback.allowEphemeralPort = true;
        slimr::McpHttpServer badHost(app, nonLoopback);
        std::string err;
        CHECK(badHost.start(&err) == false);
        CHECK(err.find("Non-loopback") != std::string::npos);

        slimr::HttpServerOptions unflaggedEphemeral;
        unflaggedEphemeral.host = "127.0.0.1";
        unflaggedEphemeral.port = 0;
        slimr::McpHttpServer badPort(app, unflaggedEphemeral);
        CHECK(badPort.start(&err) == false);

        slimr::enable_http(app, slimr::HttpServerOptions{.host = "127.0.0.1", .port = 8083});
        char a0[] = "brave-history-mcp";
        char a1[] = "http";
        char a2[] = "--host";
        char a3[] = "0.0.0.0";
        char* badArgv[] = {a0, a1, a2, a3, nullptr};
        CHECK(app.run(4, badArgv) == 1);

        char s1[] = "status";
        char* statusArgv[] = {a0, s1, nullptr};
        std::stringstream ss;
        auto* oldOut = std::cout.rdbuf(ss.rdbuf());
        const int rc = app.run(2, statusArgv);
        std::cout.rdbuf(oldOut);
        CHECK(rc == 0);
        auto parsedStatus = slimr::parse_json<slimr::Json>(ss.str());
        REQUIRE(parsedStatus.has_value());
        CHECK((*parsedStatus)["version"].get<std::string>() == "2.0.0");
    }

    TEST_CASE("ephemeral Streamable HTTP server serves discovery, tools and concurrent reads") {
        auto fixture = createFixture({
            {"Default", {
                {.url = "https://example.com/desk", .title = "Standing desk", .visits = {{.at = "2026-03-01T10:00:00Z"}}},
                {.url = "https://example.com/chair", .title = "Ergonomic chair", .visits = {{.at = "2026-03-02T11:00:00Z"}}},
            }},
        });
        auto app = make_brave_history_app();

        slimr::HttpServerOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 0;
        opts.allowEphemeralPort = true;
        slimr::McpHttpServer srv(app, opts);
        std::string err;
        REQUIRE(srv.start(&err));
        const uint16_t port = srv.boundPort();
        REQUIRE(port != 0);

        struct ServerStop {
            slimr::McpHttpServer& server;
            ~ServerStop() { server.stop(); }
        } stopGuard{srv};

        // 1. Legacy initialize returns HTTP 404 / JSON-RPC -32601
        {
            const auto resp = http_post_mcp(port, "initialize", "", make_mcp_request(10, "initialize"));
            CHECK(resp.status == 404);
            auto j = slimr::parse_json<slimr::Json>(resp.body);
            REQUIRE(j.has_value());
            CHECK((*j)["error"]["code"].get<int64_t>() == static_cast<int64_t>(slimr::McpErrorCode::MethodNotFound));
        }

        // 2. server/discover
        {
            const auto resp = http_post_mcp(port, "server/discover", "", make_mcp_request(11, "server/discover"));
            CHECK(resp.status == 200);
            auto j = slimr::parse_json<slimr::Json>(resp.body);
            REQUIRE(j.has_value());
            CHECK((*j)["result"]["supportedVersions"][0].get<std::string>() == slimr::kMcpProtocolVersion);
            CHECK((*j)["result"]["_meta"]["io.modelcontextprotocol/serverInfo"]["name"].get<std::string>() == "brave-history-mcp");
        }

        // 3. tools/list
        {
            const auto resp = http_post_mcp(port, "tools/list", "", make_mcp_request(12, "tools/list"));
            CHECK(resp.status == 200);
            auto j = slimr::parse_json<slimr::Json>(resp.body);
            REQUIRE(j.has_value());
            CHECK((*j)["result"]["tools"].get<slimr::Json::array_t>().size() == 4);
        }

        // 4. search, list, visits, status over HTTP
        {
            const auto sResp = http_post_mcp(
                port, "tools/call", "search",
                make_tool_call(13, "search", slimr::Json::object_t{{"words", slimr::Json::array_t{"standing"}}})
            );
            CHECK(sResp.status == 200);
            auto sj = slimr::parse_json<slimr::Json>(sResp.body);
            REQUIRE(sj.has_value());
            CHECK((*sj)["result"]["isError"].get<bool>() == false);
            auto parsed = slimr::parse_json<slimr::Json>((*sj)["result"]["content"][0]["text"].get<std::string>());
            REQUIRE(parsed.has_value());
            CHECK((*parsed)["pages"].get<slimr::Json::array_t>().size() == 1);

            const auto lResp = http_post_mcp(
                port, "tools/call", "list",
                make_tool_call(14, "list", slimr::Json::object_t{})
            );
            CHECK(lResp.status == 200);
            auto lj = slimr::parse_json<slimr::Json>(lResp.body);
            REQUIRE(lj.has_value());
            auto lParsed = slimr::parse_json<slimr::Json>((*lj)["result"]["content"][0]["text"].get<std::string>());
            REQUIRE(lParsed.has_value());
            CHECK((*lParsed)["pages"].get<slimr::Json::array_t>().size() == 2);

            const auto vResp = http_post_mcp(
                port, "tools/call", "visits",
                make_tool_call(15, "visits", slimr::Json::object_t{{"url", "https://example.com/desk"}})
            );
            CHECK(vResp.status == 200);
            auto vj = slimr::parse_json<slimr::Json>(vResp.body);
            REQUIRE(vj.has_value());
            auto vParsed = slimr::parse_json<slimr::Json>((*vj)["result"]["content"][0]["text"].get<std::string>());
            REQUIRE(vParsed.has_value());
            CHECK((*vParsed)["visits"].get<slimr::Json::array_t>().size() == 1);

            const auto stResp = http_post_mcp(
                port, "tools/call", "status",
                make_tool_call(16, "status", slimr::Json::object_t{})
            );
            CHECK(stResp.status == 200);
            auto stj = slimr::parse_json<slimr::Json>(stResp.body);
            REQUIRE(stj.has_value());
            CHECK((*stj)["result"]["isError"].get<bool>() == false);
        }

        // 5. Concurrent HTTP callers across threads
        std::atomic<int> failures{0};
        std::vector<std::thread> workers;
        for (int t = 0; t < 4; ++t) {
            workers.emplace_back([&, t]() {
                for (int i = 0; i < 5; ++i) {
                    const auto r = http_post_mcp(
                        port, "tools/call", "search",
                        make_tool_call(100 + t * 10 + i, "search", slimr::Json::object_t{{"words", slimr::Json::array_t{"desk"}}})
                    );
                    if (r.status != 200) {
                        failures.fetch_add(1);
                        continue;
                    }
                    auto j = slimr::parse_json<slimr::Json>(r.body);
                    if (!j || (*j)["result"]["isError"].get<bool>()) {
                        failures.fetch_add(1);
                    }
                }
            });
        }
        for (auto& w : workers) w.join();
        CHECK(failures.load() == 0);
    }
}
