/// @file TestStreamAccumulator.cpp
/// @brief Unit test verifying RxStreamAccumulator framing, fragmentation, and edge cases.

#include "PelcoDFrame.h"
#include "RxStreamAccumulator.h"

#include <cassert>
#include <iostream>
#include <vector>

void testSingleFrame()
{
    PelcoD::RxStreamAccumulator acc;
    assert(acc.size() == 0U);

    const auto frame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x02U, 0x20U, 0x00U);
    assert(frame.size() == 7U);

    const auto extracted = acc.push(frame);
    assert(extracted.size() == 1U);
    assert(extracted[0] == frame);
    assert(acc.size() == 0U);
}

void testFragmentedFrame()
{
    PelcoD::RxStreamAccumulator acc;

    const auto frame = PelcoD::PelcoDFrame::createFrame(2U, 0x00U, 0x04U, 0x00U, 0x30U);
    const std::vector<std::uint8_t> chunk1(frame.begin(), frame.begin() + 3);
    const std::vector<std::uint8_t> chunk2(frame.begin() + 3, frame.end());

    auto res1 = acc.push(chunk1);
    assert(res1.empty());
    assert(acc.size() == 3U);

    auto res2 = acc.push(chunk2);
    assert(res2.size() == 1U);
    assert(res2[0] == frame);
    assert(acc.size() == 0U);
}

void testMultipleFramesInOneChunk()
{
    PelcoD::RxStreamAccumulator acc;

    const auto frame1 = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x02U, 0x20U, 0x00U);
    const auto frame2 = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x04U, 0x00U, 0x20U);

    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), frame1.begin(), frame1.end());
    stream.insert(stream.end(), frame2.begin(), frame2.end());

    const auto extracted = acc.push(stream);
    assert(extracted.size() == 2U);
    assert(extracted[0] == frame1);
    assert(extracted[1] == frame2);
    assert(acc.size() == 0U);
}

void testGarbagePreamble()
{
    PelcoD::RxStreamAccumulator acc;

    const auto frame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x00U, 0x00U, 0x00U);
    std::vector<std::uint8_t> noisyData { 0x00U, 0x12U, 0x34U, 0xAAU };
    noisyData.insert(noisyData.end(), frame.begin(), frame.end());

    const auto extracted = acc.push(noisyData);
    assert(extracted.size() == 1U);
    assert(extracted[0] == frame);
    assert(acc.size() == 0U);
}

void testBufferOverflowReset()
{
    PelcoD::RxStreamAccumulator acc(100U);
    assert(acc.maxBufferSize() == 100U);

    std::vector<std::uint8_t> garbage(120U, 0x11U);
    const auto extracted = acc.push(garbage);
    assert(extracted.empty());
    assert(acc.size() == 0U);

    const auto validFrame = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x08U, 0x00U, 0x00U);
    const auto extractedValid = acc.push(validFrame);
    assert(extractedValid.size() == 1U);
    assert(extractedValid[0] == validFrame);
}

int main()
{
    std::cout << "[TestStreamAccumulator] Running tests...\n";
    testSingleFrame();
    testFragmentedFrame();
    testMultipleFramesInOneChunk();
    testGarbagePreamble();
    testBufferOverflowReset();
    std::cout << "[TestStreamAccumulator] All tests passed successfully.\n";
    return 0;
}
