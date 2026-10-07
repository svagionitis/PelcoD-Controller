/// @file CredentialStore.cpp
/// @brief Implementation of the authoritative ONVIF user store.

#include "CredentialStore.h"
#include "PasswordPolicy.h"

#include <openssl/crypto.h>

#include <algorithm>
#include <mutex>

namespace Onvif {

namespace {

    constexpr std::size_t kMaxNameLength { 32U };

    /// @brief Overwrites a secret's bytes before it is released.
    void wipe(std::string& secret) noexcept
    {
        if (!secret.empty()) {
            OPENSSL_cleanse(secret.data(), secret.size());
        }
        secret.clear();
    }

    /// @brief Returns true if the character is allowed in a user name.
    [[nodiscard]] bool isNameChar(char ch) noexcept
    {
        const bool lower { (ch >= 'a') && (ch <= 'z') };
        const bool upper { (ch >= 'A') && (ch <= 'Z') };
        const bool digit { (ch >= '0') && (ch <= '9') };
        return lower || upper || digit || (ch == '.') || (ch == '_') || (ch == '-');
    }

} // namespace

CredentialStore::CredentialStore(const std::vector<OnvifUser>& seed)
{
    reset(seed);
}

CredentialStore::~CredentialStore()
{
    wipeAll();
}

bool CredentialStore::isValidName(std::string_view username) noexcept
{
    if (username.empty() || (username.size() > kMaxNameLength)) {
        return false;
    }
    return std::all_of(username.begin(), username.end(), [](char ch) { return isNameChar(ch); });
}

std::size_t CredentialStore::countAdmins() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        m_users.begin(), m_users.end(), [](const OnvifUser& u) { return u.level == OnvifUserLevel::Administrator; }));
}

void CredentialStore::wipeAll() noexcept
{
    for (auto& u : m_users) {
        wipe(u.password);
    }
    m_users.clear();
}

std::optional<OnvifUser> CredentialStore::find(std::string_view username) const
{
    const std::shared_lock lock { m_mutex };
    const auto it { std::find_if(
        m_users.begin(), m_users.end(), [username](const OnvifUser& u) { return u.username == username; }) };
    if (it == m_users.end()) {
        return std::nullopt;
    }
    return *it;
}

bool CredentialStore::upsert(const OnvifUser& user)
{
    if (!isValidName(user.username) || (user.level == OnvifUserLevel::Anonymous)) {
        return false;
    }
    const std::unique_lock lock { m_mutex };
    const auto it { std::find_if(
        m_users.begin(), m_users.end(), [&user](const OnvifUser& u) { return u.username == user.username; }) };
    if (it == m_users.end()) {
        m_users.push_back(user);
        return true;
    }
    const bool demotesAdmin { (it->level == OnvifUserLevel::Administrator)
        && (user.level != OnvifUserLevel::Administrator) };
    if (demotesAdmin && (countAdmins() <= 1U)) {
        return false;
    }
    wipe(it->password);
    *it = user;
    return true;
}

bool CredentialStore::erase(std::string_view username)
{
    const std::unique_lock lock { m_mutex };
    const auto it { std::find_if(
        m_users.begin(), m_users.end(), [username](const OnvifUser& u) { return u.username == username; }) };
    if (it == m_users.end()) {
        return false;
    }
    if ((it->level == OnvifUserLevel::Administrator) && (countAdmins() <= 1U)) {
        return false;
    }
    wipe(it->password);
    static_cast<void>(m_users.erase(it));
    return true;
}

void CredentialStore::reset(const std::vector<OnvifUser>& seed)
{
    const std::unique_lock lock { m_mutex };
    wipeAll();
    for (const auto& u : seed) {
        const bool duplicate { std::any_of(
            m_users.begin(), m_users.end(), [&u](const OnvifUser& e) { return e.username == u.username; }) };
        if (isValidName(u.username) && (u.level != OnvifUserLevel::Anonymous) && !duplicate) {
            m_users.push_back(u);
        }
    }
}

std::vector<OnvifUser> CredentialStore::list() const
{
    const std::shared_lock lock { m_mutex };
    std::vector<OnvifUser> out {};
    out.reserve(m_users.size());
    for (const auto& u : m_users) {
        out.push_back(OnvifUser { u.username, std::string {}, u.level });
    }
    return out;
}

std::size_t CredentialStore::adminCount() const
{
    const std::shared_lock lock { m_mutex };
    return countAdmins();
}

bool CredentialStore::hasDefaultPassword() const
{
    const std::shared_lock lock { m_mutex };
    return std::any_of(m_users.begin(), m_users.end(), [](const OnvifUser& u) {
        return u.password.empty() || (u.password == u.username) || PasswordPolicy::isCommonDefault(u.password);
    });
}

} // namespace Onvif
