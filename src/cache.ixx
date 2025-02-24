module;

export module cache;

import <iostream>;
import <unordered_map>;
import <filesystem>;
import <vector>;
import <regex>;
import <string>;


namespace hierview
{
namespace fs = std::filesystem;
using FrameMap = std::unordered_map<int, std::vector<fs::directory_entry>>;


struct PathHasher
{
    std::size_t operator()(const fs::path& p) const
    {
        return std::hash<std::string>{}(fs::canonical(p).string());
    }
};


struct PathEqual
{
    bool operator()(const fs::path& p1, const fs::path& p2) const
    {
        return fs::equivalent(p1, p2);
    }
};


std::unordered_map<fs::path, FrameMap, PathHasher, PathEqual> cache;


export int last_index(const std::string& str)
{
    static std::regex pat("(\\d+)");
    std::smatch match;
    if (std::regex_search(str, match, pat))
        return std::stoi(match[match.size() - 1].str());
    return -1;
}


void map_indices(const fs::path& dir, FrameMap& entries)
{
    if (!fs::is_directory(dir))
    {
        std::cerr << dir << " is not a directory" << std::endl;
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir))
    {
        int i = last_index(entry.path().filename().string());
        if (i < 0)
            continue;
        auto it = entries.find(i);
        if (it == entries.end())
            entries[i] = {entry};
        else
            it->second.push_back(entry);
    }
}


export FrameMap& get_entries(const fs::path& dir)
{
    auto [it, _] = cache.try_emplace(dir);
    map_indices(dir, it->second);
    return it->second;
}


export void clear_cache()
{
    cache.clear();
}
}
