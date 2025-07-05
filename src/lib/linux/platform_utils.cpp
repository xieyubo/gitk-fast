module;

#include <functional>
#include <stdexcept>
#include <string>

export module gitkf:platform_utils;

export std::string ExternRun(const std::string& commandLine, const char* workingDir)
{
    throw std::runtime_error { "Not Implemented" };
}

export void ExternRun(
    const std::string& commandLine, const char* workingDir, const std::function<bool(char*, size_t)>& onData)
{
    throw std::runtime_error { "Not Implemented" };
}

export std::string get_current_app_full_path()
{
    throw std::runtime_error { "Not Implemented" };
}

export void RunAsDaemon(const std::string& cmd)
{
    throw std::runtime_error { "Not Implemented" };
}

export void OpenUrl(const std::string& url)
{
    throw std::runtime_error { "Not Implemented" };
}