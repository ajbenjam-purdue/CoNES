#ifndef CONES_CORE_PLATFORM_HPP
#define CONES_CORE_PLATFORM_HPP

#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace cones {

/**
 * @brief Returns the absolute path to the currently running executable.
 * This is more reliable than argv[0] when the executable is called from PATH.
 */
inline std::filesystem::path get_executable_path() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    DWORD size = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (size == 0 || size == MAX_PATH) {
        return "";
    }
    return std::filesystem::path(buffer);
#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        return std::filesystem::path(buffer);
    }
    return "";
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        // Resolve symlinks to get the actual path
        return std::filesystem::weakly_canonical(buffer.data());
    }
    return "";
#else
    return "";
#endif
}

/**
 * @brief Wraps a command string for execution via std::system.
 * On Windows, this wraps the entire command in double quotes to handle
 * paths with spaces and internal quotes correctly.
 */
inline std::string wrap_command(const std::string& cmd) {
#ifdef _WIN32
    return "\"" + cmd + "\"";
#else
    return cmd;
#endif
}

} // namespace cones

#endif
