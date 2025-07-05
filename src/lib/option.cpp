module;

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

export module gitkf:option;

constexpr int kDefaultPort = 13324;

static bool ParseFlag(int i, char* argv[], const char* key, bool& val)
{
    if (!strcmp(argv[i], key)) {
        val = true;
        return true;
    }
    return false;
}

static bool ParseOption(int& i, int argc, char* argv[], const char* key, std::string& val)
{
    if (!strcmp(argv[i], key)) {
        if (i + 1 < argc) {
            val = argv[++i];
            return true;
        } else {
            throw std::runtime_error { std::format("Missing argument for {}.", argv[i]) };
        }
    }
    return false;
}

template <typename T>
bool ParseOption(int& i, int argc, char* argv[], const char* key, std::vector<T>& vals)
{
    T val {};
    if (ParseOption(i, argc, argv, key, val)) {
        vals.emplace_back(std::move(val));
        return true;
    }
    return false;
}

static bool IsGitId(const std::string& hash)
{
    if (hash.length() != 40) {
        return false;
    }

    for (auto ch : hash) {
        if (!std::isxdigit(ch)) {
            return false;
        }
    }
    return true;
}

export struct Option {
    int port { kDefaultPort };
    bool serverMode {};
    std::string repoPath { std::filesystem::current_path().string() };
    std::string commitId {};
    std::string follow {};
    std::vector<std::string> authors {};
    std::string wwwroot {};
    bool printVersion {};

    static Option Parse(int argc, char* argv[])
    {
        Option option {};
        for (int i = 0; i < argc; ++i) {
            if (*argv[i] == '-') {
                if (!ParseFlag(i, argv, "--server", option.serverMode) && !ParseFlag(i, argv, "-v", option.printVersion)
                    && !ParseFlag(i, argv, "--version", option.printVersion)
                    && !ParseOption(i, argc, argv, "--repo", option.repoPath)
                    && !ParseOption(i, argc, argv, "--wwwroot", option.wwwroot)
                    && !ParseOption(i, argc, argv, "--author", option.authors)) {
                    throw std::runtime_error { std::format("Unknonw option: {}", argv[i]) };
                }
            } else {
                auto str = std::string { argv[i] };
                if (IsGitId(str)) {
                    if (option.commitId.empty()) {
                        option.commitId = std::move(str);
                    } else {
                        throw std::runtime_error { "Only support one commit id." };
                    }
                } else {
                    // Treat as follow path.
                    if (option.follow.empty()) {
                        option.follow = std::filesystem::weakly_canonical(argv[i]).string();
                    } else {
                        throw std::runtime_error { "Only support one path." };
                    }
                }
            }
        }
        return option;
    }
};
