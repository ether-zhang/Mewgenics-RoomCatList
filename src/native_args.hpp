#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace roomcats {
inline std::string NormalizeModPath(std::string path, const std::string& base = {}) {
    std::replace(path.begin(), path.end(), '\\', '/');
    auto value = std::filesystem::u8path(path);
    if (value.is_relative() && !base.empty()) value = std::filesystem::u8path(base) / value;
    path = value.lexically_normal().generic_u8string();
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) {return static_cast<char>(std::tolower(c));});
    while (path.size() > 3 && path.back() == '/') path.pop_back();
    return path;
}

inline std::vector<std::string> AddOwnModPath(const std::vector<std::string>& original, const std::string& folder, const std::string& base = {}) {
    if (original.empty() || folder.empty()) return original;
    auto args = original;
    const auto normalized = NormalizeModPath(folder, base);
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (NormalizeModPath(args[i]) != "-modpaths") continue;
        for (auto j = i + 1; j < args.size() && (args[j].empty() || args[j][0] != '-'); ++j)
            if (NormalizeModPath(args[j], base) == normalized) return args;
        args.insert(args.begin() + i + 1, folder);
        return args;
    }
    args.insert(args.begin() + 1, {"-modpaths", folder});
    return args;
}
}
