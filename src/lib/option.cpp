module;

#include <filesystem>
#include <string>

export module gitkf:option;

static const int kDefaultPort = 13324;

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

export struct Option {
    int port { kDefaultPort };
    bool serverMode {};
    std::string repoPath { std::filesystem::current_path().string() };
    std::string follow {};
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
                    && !ParseOption(i, argc, argv, "--wwwroot", option.wwwroot)) {
                    throw std::runtime_error { std::format("Unknonw option: {}", argv[i]) };
                }
            } else if (option.follow.empty()) {
                option.follow = std::filesystem::weakly_canonical(argv[i]).string();
            } else {
                throw std::runtime_error { "Only support one path." };
            }
        }
        return option;
    }
};
