module;

#include <array>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

export module gitkf:platform_utils;

export std::string get_current_app_full_path()
{
    std::vector<char> buffer {};
    buffer.resize(MAX_PATH);
    while (true) {
        auto res = GetModuleFileNameA(nullptr, buffer.data(), buffer.size());
        if (!res) {
            throw std::runtime_error { "Get application full path failed." };
        } else if (res < buffer.size()) {
            return buffer.data();
        } else {
            buffer.resize(2 * buffer.size());
        }
    }
}

export std::string ExternRun(const std::string& commandLine, const char* workingDir)
{
    STARTUPINFOA si { .cb = sizeof(si) };
    PROCESS_INFORMATION pi {};
    auto pReadHandle = std::unique_ptr<void, decltype(&CloseHandle)> { nullptr, &CloseHandle };
    auto pWriteHandle = std::unique_ptr<void, decltype(&CloseHandle)> { nullptr, &CloseHandle };
    std::string output {};

    // Create a pipe for the child process's stdout.
    SECURITY_ATTRIBUTES sa {
        .nLength = sizeof(sa),
        .bInheritHandle = true,
    };

    if (!::CreatePipe(std::out_ptr(pReadHandle), std::out_ptr(pWriteHandle), &sa, /*nSize=*/0)) {
        auto err = GetLastError();
        throw std::runtime_error { std::format(
            "'Create pipe for command '{}' failed, error code: {}.'", commandLine, err) };
    }

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = pWriteHandle.get();
    si.hStdError = pWriteHandle.get();

    if (!::CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, true, CREATE_NO_WINDOW, nullptr,
            workingDir, &si, &pi)) {
        auto err = GetLastError();
        throw std::runtime_error { std::format(
            "'Create process failed for command '{}', error code: {}.'", commandLine, err) };
    }

    // Close write handle from parent, otherwise ReadFile() will hang even child process closes it already.
    pWriteHandle = nullptr;

    // Read output data.
    while (true) {
        std::array<char, 1024> buffer;
        DWORD readed {};
        if (!::ReadFile(pReadHandle.get(), buffer.data(), buffer.size(), &readed, /*lpOoverlapped=*/nullptr)
            || !readed) {
            break;
        } else {
            output.append(buffer.data(), readed);
        }
    }

    // Wait child process exit.
    ::WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode {};
    ::GetExitCodeProcess(pi.hProcess, &exitCode);

    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);

    if (exitCode) {
        throw std::runtime_error { std::format(
            "Command '{}' failed, output: {}, exit code: {}.", commandLine, output, exitCode) };
    }

    return output;
}

export void RunAsDaemon(const std::string& cmd)
{
    // No server is running, start it.
    STARTUPINFOA si { .cb = sizeof(si) };
    PROCESS_INFORMATION pi {};
    if (!CreateProcessA(
            nullptr, (char*)cmd.c_str(), nullptr, nullptr, false, DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
        auto err = GetLastError();
        throw std::runtime_error { std::format("Start server failed, error code: {}", err) };
    }
}

export void OpenUrl(const std::string& url)
{
    ShellExecuteA(0, 0, url.c_str(), 0, 0, SW_SHOW);
}