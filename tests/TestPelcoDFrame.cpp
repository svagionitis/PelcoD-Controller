/// @file TestPelcoDFrame.cpp
/// @brief Unit tests for Pelco-D frame checksum, creation, validation, and stream splitting.

#include "PelcoDFrame.h"

#include <cassert>
#include <iostream>
#include <vector>

void testChecksumCalculation()
{
    // Spec Page 18: Message 1 (Camera 2, Pan Left)
    // 0xFF, 0x02, 0x00, 0x04, 0x20, 0x00 -> Checksum 0x26
    const std::vector<std::uint8_t> p1 { 0x02U, 0x00U, 0x04U, 0x20U, 0x00U };
    assert(PelcoD::PelcoDFrame::calculateChecksum(p1) == 0x26U);

    // Spec Page 18: Message 2 (Camera 2, Stop)
    // 0xFF, 0x02, 0x00, 0x00, 0x20, 0x00 -> Checksum 0x22
    const std::vector<std::uint8_t> p2 { 0x02U, 0x00U, 0x00U, 0x20U, 0x00U };
    assert(PelcoD::PelcoDFrame::calculateChecksum(p2) == 0x22U);

    // Spec Page 18: Message 3 (Camera 10, Camera on, Focus far, Tilt Down)
    // 0xFF, 0x0A, 0x88, 0x90, 0x00, 0x20 -> Checksum 0x42
    const std::vector<std::uint8_t> p3 { 0x0AU, 0x88U, 0x90U, 0x00U, 0x20U };
    assert(PelcoD::PelcoDFrame::calculateChecksum(p3) == 0x42U);

    // Modulo 256 overflow check
    const std::vector<std::uint8_t> overflow { 0xFFU, 0x02U, 0x00U, 0x00U, 0x00U };
    assert(PelcoD::PelcoDFrame::calculateChecksum(overflow) == 0x01U);
}

void testCreateFrame()
{
    const auto frame = PelcoD::PelcoDFrame::createFrame(0x02U, 0x00U, 0x04U, 0x20U, 0x00U);
    assert(frame.size() == 7U);
    assert(frame[0] == 0xFFU);
    assert(frame[1] == 0x02U);
    assert(frame[2] == 0x00U);
    assert(frame[3] == 0x04U);
    assert(frame[4] == 0x20U);
    assert(frame[5] == 0x00U);
    assert(frame[6] == 0x26U);
}

void testFrameValidation()
{
    // Valid 4-byte general response
    const std::vector<std::uint8_t> valid4 { 0xFFU, 0x01U, 0x00U, 0x01U };
    assert(PelcoD::PelcoDFrame::isValidFrame(valid4));

    // Valid 7-byte frame
    const auto valid7 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x20U, 0x00U, 0x00U);
    assert(PelcoD::PelcoDFrame::isValidFrame(valid7));

    // Invalid 7-byte frame with bad checksum
    auto bad7 = valid7;
    bad7[6] = 0xAAU;
    assert(!PelcoD::PelcoDFrame::isValidFrame(bad7));

    // Valid 18-byte query response
    std::vector<std::uint8_t> valid18(18U, 0x00U);
    valid18[0] = 0xFFU;
    valid18[1] = 0x01U;
    assert(PelcoD::PelcoDFrame::isValidFrame(valid18));
}

void testStreamSplitting()
{
    // Stream with junk bytes preceding a valid frame, followed by a second frame
    std::vector<std::uint8_t> stream {
        0x12U, 0x34U, 0x56U, // Noise
        0xFFU, 0x02U, 0x00U, 0x04U, 0x20U, 0x00U, 0x26U, // Valid 7-byte
        0xFFU, 0x01U, 0x00U, 0x01U // Valid 4-byte general reply
    };

    const auto frames = PelcoD::PelcoDFrame::splitStream(stream);
    assert(frames.size() == 2U);
    assert(frames[0].size() == 7U);
    assert(frames[0][6] == 0x26U);
    assert(frames[1].size() == 4U);
}

int main()
{
    std::cout << "[TestPelcoDFrame] Running tests..." << std::endl;
    testChecksumCalculation();
    testCreateFrame();
    testFrameValidation();
    testStreamSplitting();
    std::cout << "[TestPelcoDFrame] All tests passed successfully." << std::endl;
    return 0;
}
