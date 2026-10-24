/// @file TestFuzzRegression.cpp
/// @brief Deterministic regression test driver verifying all corpus files pass without crash.

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

extern "C" int FuzzSoapRouterTest(const std::uint8_t* data, std::size_t size);
extern "C" int FuzzWsDiscoveryTest(const std::uint8_t* data, std::size_t size);
extern "C" int FuzzClientXmlTest(const std::uint8_t* data, std::size_t size);
extern "C" int FuzzAuthHeadersTest(const std::uint8_t* data, std::size_t size);
extern "C" int FuzzUrlValidatorTest(const std::uint8_t* data, std::size_t size);

namespace {

[[nodiscard]] std::vector<std::uint8_t> readSeedFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return { };
    }
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
    if (size > 0) {
        file.read(reinterpret_cast<char*>(buffer.data()), size);
    }
    return buffer;
}

void executeCorpus(const std::string& subDir, int (*harnessFunc)(const std::uint8_t*, std::size_t))
{
#if defined(FUZZ_CORPUS_ROOT)
    const std::filesystem::path corpusDir = std::filesystem::path(FUZZ_CORPUS_ROOT) / subDir;
#else
    const std::filesystem::path corpusDir = std::filesystem::path("corpus") / subDir;
#endif

    if (!std::filesystem::exists(corpusDir)) {
        GTEST_SKIP() << "Corpus directory not found: " << corpusDir.string();
    }

    std::size_t executedCount = 0U;
    for (const auto& entry : std::filesystem::directory_iterator(corpusDir)) {
        if (entry.is_regular_file()) {
            const auto data = readSeedFile(entry.path());
            const int res = harnessFunc(data.data(), data.size());
            EXPECT_EQ(res, 0) << "Failure executing seed: " << entry.path().filename().string();
            ++executedCount;
        }
    }
    EXPECT_GT(executedCount, 0U) << "No seed files found in " << corpusDir.string();
}

} // namespace

TEST(TestFuzzRegression, SoapRouterSeeds)
{
    executeCorpus("soap_router", FuzzSoapRouterTest);
}

TEST(TestFuzzRegression, WsDiscoverySeeds)
{
    executeCorpus("ws_discovery", FuzzWsDiscoveryTest);
}

TEST(TestFuzzRegression, ClientXmlSeeds)
{
    executeCorpus("client_xml", FuzzClientXmlTest);
}

TEST(TestFuzzRegression, AuthHeadersSeeds)
{
    executeCorpus("auth_headers", FuzzAuthHeadersTest);
}

TEST(TestFuzzRegression, UrlValidatorSeeds)
{
    executeCorpus("url_validator", FuzzUrlValidatorTest);
}
