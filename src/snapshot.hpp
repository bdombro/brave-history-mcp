#pragma once

#include "json.hpp"
#include "sqlite.hpp"

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace brave_history {

struct BraveProfile {
    std::string dir;
    std::optional<std::string> name;

    bool operator==(const BraveProfile& o) const = default;
};

inline std::string braveRoot() {
    const char* env = std::getenv("BRAVE_HISTORY_ROOT");
    if (env && *env) return std::string(env);
    const char* home = std::getenv("HOME");
    std::string h = home ? home : "";
    return (std::filesystem::path(h) / "Library" / "Application Support" / "BraveSoftware" / "Brave-Browser").string();
}

inline std::string cacheRoot() {
    const char* env = std::getenv("BRAVE_HISTORY_CACHE");
    if (env && *env) return std::string(env);
    const char* home = std::getenv("HOME");
    std::string h = home ? home : "";
    return (std::filesystem::path(h) / ".local" / "share" / "brave-history").string();
}

inline int64_t get_file_mtime_ms(const std::filesystem::path& p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0) {
        return -1;
    }
#if defined(__APPLE__)
    return static_cast<int64_t>(st.st_mtimespec.tv_sec) * 1000LL + st.st_mtimespec.tv_nsec / 1000000LL;
#else
    return static_cast<int64_t>(st.st_mtim.tv_sec) * 1000LL + st.st_mtim.tv_nsec / 1000000LL;
#endif
}

inline void lockDown(const std::filesystem::path& dir) {
    chmod(dir.c_str(), 0700);
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        chmod(entry.path().c_str(), 0600);
    }
}

inline std::map<std::string, std::string> profileNames(const std::filesystem::path& root) {
    std::map<std::string, std::string> names;
    try {
        auto p = root / "Local State";
        if (!std::filesystem::exists(p)) return names;
        std::ifstream f(p);
        if (!f.is_open()) return names;
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto parsed = slimr::parse_json<slimr::Json>(content);
        if (!parsed || !parsed->is_object()) return names;
        if (!parsed->contains("profile") || !(*parsed)["profile"].is_object()) return names;
        auto& prof = (*parsed)["profile"];
        if (!prof.contains("info_cache") || !prof["info_cache"].is_object()) return names;
        auto& cache = prof["info_cache"];
        for (const auto& [dir, info] : cache.get<slimr::Json::object_t>()) {
            if (info.is_object() && info.contains("name") && info["name"].is_string()) {
                names[dir] = info["name"].get<std::string>();
            }
        }
    } catch (...) {
        // ignore
    }
    return names;
}

inline bool isProfileDir(const std::string& dir) {
    if (dir == "Default") return true;
    if (dir.starts_with("Profile ") && dir.size() > 8) {
        for (size_t i = 8; i < dir.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(dir[i]))) return false;
        }
        return true;
    }
    return false;
}

inline bool compareProfiles(const BraveProfile& a, const BraveProfile& b) {
    if (a.dir == "Default" && b.dir == "Default") return false;
    if (a.dir == "Default") return true;
    if (b.dir == "Default") return false;
    try {
        int numA = std::stoi(a.dir.substr(8));
        int numB = std::stoi(b.dir.substr(8));
        return numA < numB;
    } catch (...) {
        return a.dir < b.dir;
    }
}

inline std::vector<BraveProfile> listProfiles() {
    std::filesystem::path root(braveRoot());
    if (!std::filesystem::exists(root)) {
        throw std::runtime_error("Brave data dir not found: " + root.string() + " (set BRAVE_HISTORY_ROOT to override)");
    }
    auto names = profileNames(root);
    std::vector<BraveProfile> profiles;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (!entry.is_directory()) continue;
        std::string dir = entry.path().filename().string();
        if (isProfileDir(dir) && std::filesystem::exists(entry.path() / "History")) {
            BraveProfile p{.dir = dir};
            if (names.contains(dir)) {
                p.name = names[dir];
            }
            profiles.push_back(std::move(p));
        }
    }

    std::sort(profiles.begin(), profiles.end(), compareProfiles);
    return profiles;
}

inline std::string to_lower_str(std::string_view s) {
    std::string res;
    res.reserve(s.size());
    for (char c : s) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

inline std::vector<BraveProfile> selectProfiles(const std::optional<std::string>& filter) {
    auto all = listProfiles();
    if (!filter.has_value() || filter->empty()) {
        return all;
    }
    std::string needle = to_lower_str(*filter);
    std::vector<BraveProfile> matched;
    for (const auto& p : all) {
        if (to_lower_str(p.dir) == needle || (p.name && to_lower_str(*p.name) == needle)) {
            matched.push_back(p);
        }
    }
    if (matched.empty()) {
        std::string known;
        for (size_t i = 0; i < all.size(); ++i) {
            if (i > 0) known += ", ";
            if (all[i].name) {
                known += all[i].dir + " (" + *all[i].name + ")";
            } else {
                known += all[i].dir;
            }
        }
        throw std::runtime_error("No Brave profile matches \"" + *filter + "\". Known: " + (known.empty() ? "none" : known));
    }
    return matched;
}

inline std::string ensureCachedHistory(const BraveProfile& profile) {
    std::filesystem::path root = std::filesystem::absolute(braveRoot()).lexically_normal();
    std::filesystem::path cacheDir = std::filesystem::absolute(std::filesystem::path(cacheRoot()) / profile.dir).lexically_normal();

    std::string rootSlash = root.string();
    if (!rootSlash.ends_with('/')) rootSlash += '/';
    std::string cacheDirSlash = cacheDir.string();
    if (!cacheDirSlash.ends_with('/')) cacheDirSlash += '/';

    if (cacheDirSlash.starts_with(rootSlash)) {
        throw std::runtime_error("Refusing to cache inside the Brave data dir: " + cacheDir.string());
    }

    std::filesystem::path source = root / profile.dir / "History";
    std::filesystem::path cached = cacheDir / "History";

    int64_t sourceMtimeMs = 0;
    const std::vector<std::string> sidecars = {"", "-wal", "-journal"};
    for (const auto& suffix : sidecars) {
        int64_t m = get_file_mtime_ms(source.string() + suffix);
        if (m > sourceMtimeMs) {
            sourceMtimeMs = m;
        }
    }

    int64_t cachedMtimeMs = get_file_mtime_ms(cached);
    if (sourceMtimeMs <= cachedMtimeMs) {
        try {
            lockDown(cacheDir);
        } catch (...) {}
        return cached.string();
    }

    // Staging using mkdtemp
    std::filesystem::create_directories(cacheDir.parent_path());
    chmod(cacheDir.parent_path().c_str(), 0700);

    std::string tmpl = (cacheDir.parent_path() / (cacheDir.filename().string() + ".tmp-XXXXXX")).string();
    std::vector<char> tmpl_buf(tmpl.begin(), tmpl.end());
    tmpl_buf.push_back('\0');

    char* staging_res = mkdtemp(tmpl_buf.data());
    if (!staging_res) {
        throw std::runtime_error("Failed to create temporary directory for history staging");
    }
    std::filesystem::path staging(staging_res);

    try {
        for (const auto& suffix : sidecars) {
            std::filesystem::path srcFile = source.string() + suffix;
            std::error_code ec;
            if (std::filesystem::exists(srcFile, ec)) {
                std::filesystem::copy_file(srcFile, staging / ("History" + suffix), std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) throw std::system_error(ec, "Failed to copy " + srcFile.string());
            }
        }

        // Open the copy read-write once so SQLite folds any WAL or hot journal into it; later reads are read-only.
        {
            slimr::SqliteDb db((staging / "History").string(), slimr::SqliteOptions{.readOnly = false, .enableDefaultPragmas = false});
            (void)db.pluck<int64_t>("SELECT count(*) FROM sqlite_master");
            db.close();
        }

        lockDown(staging);
        std::error_code ec;
        std::filesystem::remove_all(cacheDir, ec);
        std::filesystem::rename(staging, cacheDir);

        struct timeval times[2];
        times[0].tv_sec = static_cast<time_t>(sourceMtimeMs / 1000);
        times[0].tv_usec = static_cast<suseconds_t>((sourceMtimeMs % 1000) * 1000);
        times[1] = times[0];
        utimes(cached.c_str(), times);

        return cached.string();
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove_all(staging, ec);
        throw;
    }
}

} // namespace brave_history
