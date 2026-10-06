#pragma once

/// @file CredentialStore.h
/// @brief Authoritative, thread-safe store of ONVIF server user accounts.

#include "OnvifTypes.h"

#include <cstddef>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Onvif {

/// @class CredentialStore
/// @brief Single source of truth for server-side ONVIF users and their secrets.
/// @details Enforces the invariants required by ONVIF Core §5.9.4:
///          - user names are 1..32 characters of [A-Za-z0-9._-];
///          - the Anonymous level cannot be assigned to an account;
///          - the last Administrator can be neither deleted nor demoted.
///          Passwords are retained in memory in plaintext because the UsernameToken
///          PasswordDigest and HTTP Digest algorithms require them (secure persistence is
///          tracked separately as review finding C6). Overwritten and erased secrets are
///          wiped with OPENSSL_cleanse.
/// @note All member functions are thread-safe (readers share, writers are exclusive).
class CredentialStore {
public:
    /// @brief Constructs the store from an initial user list.
    /// @param[in] seed Initial users. Entries violating the name or level rules are skipped.
    explicit CredentialStore(const std::vector<OnvifUser>& seed);

    /// @brief Destructor wipes all stored secrets.
    ~CredentialStore();

    CredentialStore(const CredentialStore&) = delete;
    CredentialStore& operator=(const CredentialStore&) = delete;
    CredentialStore(CredentialStore&&) = delete;
    CredentialStore& operator=(CredentialStore&&) = delete;

    /// @brief Looks up a user including its password.
    /// @param[in] username Exact (case-sensitive) user name.
    /// @return The user record, or std::nullopt if not present.
    /// @note The returned copy contains the secret; callers must not log or persist it.
    [[nodiscard]] std::optional<OnvifUser> find(std::string_view username) const;

    /// @brief Creates or replaces a user.
    /// @param[in] user User record to store.
    /// @return True on success; false if the name is invalid, the level is Anonymous, or the
    ///         change would demote the last Administrator.
    [[nodiscard]] bool upsert(const OnvifUser& user);

    /// @brief Deletes a user.
    /// @param[in] username User to delete.
    /// @return True if deleted; false if absent or it is the last Administrator.
    [[nodiscard]] bool erase(std::string_view username);

    /// @brief Replaces all users (used by SetSystemFactoryDefault).
    /// @param[in] seed New user list, validated as in the constructor.
    void reset(const std::vector<OnvifUser>& seed);

    /// @brief Lists users with passwords blanked.
    /// @return Copies of all user records whose password field is empty.
    [[nodiscard]] std::vector<OnvifUser> list() const;

    /// @brief Counts Administrator accounts.
    /// @return Number of users with OnvifUserLevel::Administrator.
    [[nodiscard]] std::size_t adminCount() const;

    /// @brief Reports whether any account still uses a trivially guessable password.
    /// @details A password is considered default if it is empty or equal to the user name
    ///          (covers the shipped admin/admin and operator/operator accounts).
    /// @return True if at least one such account exists.
    [[nodiscard]] bool hasDefaultPassword() const;

    /// @brief Validates a user name against the store's naming rule.
    /// @param[in] username Candidate name.
    /// @return True if 1..32 characters, all in [A-Za-z0-9._-].
    [[nodiscard]] static bool isValidName(std::string_view username) noexcept;

private:
    [[nodiscard]] std::size_t countAdmins() const noexcept;
    void wipeAll() noexcept;

    mutable std::shared_mutex m_mutex {};
    std::vector<OnvifUser> m_users {};
};

} // namespace Onvif
