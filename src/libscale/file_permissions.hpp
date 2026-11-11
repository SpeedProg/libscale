#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace libscale::internal {

/**
 * @brief Restrict a file so that only its owner can read and write it.
 *
 * On POSIX platforms this is the classic owner-only mode (chmod 600).
 * Windows has no POSIX mode bits: its std::filesystem implementation only
 * maps the read-only attribute, so real protection requires ACL work. The
 * owner-only DACL is applied through the built-in icacls tool
 * (/inheritance:r removes inherited ACEs, /grant:r installs a single
 * full-control grant for the current user). icacls ships with every
 * supported Windows version and gets the ACL quirks right, which keeps
 * this header free of unsafe Win32 security API code.
 *
 * Hardening is best-effort by design: a failure is logged by the caller and
 * never blocks opening the database, matching how the POSIX branch has
 * always been wired. Note that files created inside the per-user profile
 * already default to owner + SYSTEM/Administrators, so this defends against
 * inherited group/user grants on misconfigured or shared machines.
 *
 * @param path File to restrict.
 * @return true when the restriction was applied (or confirmed), false on failure.
 */
inline auto restrict_permissions_to_owner(const std::filesystem::path& path) -> bool {
#ifdef _WIN32
    // Runs through cmd.exe, so the "> nul" redirection relies on COMSPEC
    // resolving under MSYS2 (works on GitHub's windows runners).
    char const* const username = std::getenv("USERNAME");
    if (username == nullptr || *username == '\0') {
        return false;
    }
    std::string const command = "icacls \"" + path.string() + "\" /inheritance:r /grant:r \"" +
                                username + ":(F)\" > nul 2>&1";
    return std::system(command.c_str()) == 0;
#else
    std::error_code error;
    std::filesystem::permissions(
        path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, error);
    return !error;
#endif
}

}  // namespace libscale::internal
