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

    // Invalid 4-byte general response with bad checksum (0x01 + 0x05 = 0x06 != 0x99)
    const std::vector<std::uint8_t> bad4 { 0xFFU, 0x01U, 0x05U, 0x99U };
    assert(!PelcoD::PelcoDFrame::isValidFrame(bad4));

    // Valid 7-byte frame
    const auto valid7 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x20U, 0x00U, 0x00U);
    assert(PelcoD::PelcoDFrame::isValidFrame(valid7));

    // Invalid 7-byte frame with bad checksum
    auto bad7 = valid7;
    bad7[6] = 0xAAU;
    assert(!PelcoD::PelcoDFrame::isValidFrame(bad7));

    // Valid 18-byte query response (empty/null padded)
    std::vector<std::uint8_t> valid18(18U, 0x00U);
    valid18[0] = 0xFFU;
    valid18[1] = 0x01U;
    valid18[17] = PelcoD::PelcoDFrame::calculateChecksum(&valid18[1], 16U);
    assert(PelcoD::PelcoDFrame::isValidFrame(valid18));

    // Valid 18-byte query response with ASCII model text
    auto valid18Text = valid18;
    valid18Text[2] = 'C';
    valid18Text[3] = 'A';
    valid18Text[4] = 'M';
    valid18Text[5] = '1';
    valid18Text[17] = PelcoD::PelcoDFrame::calculateChecksum(&valid18Text[1], 16U);
    assert(PelcoD::PelcoDFrame::isValidFrame(valid18Text));

    // Invalid 18-byte query response: non-printable control byte
    auto bad18Ctrl = valid18Text;
    bad18Ctrl[6] = 0x10U; // Non-printable byte < 32
    assert(!PelcoD::PelcoDFrame::isValidFrame(bad18Ctrl));

    // Invalid 18-byte query response: character after null terminator
    auto bad18AfterNull = valid18Text;
    bad18AfterNull[6] = 0x00U; // Null terminator
    bad18AfterNull[7] = 'X';   // Stray char after null
    assert(!PelcoD::PelcoDFrame::isValidFrame(bad18AfterNull));

    // Invalid 18-byte query response: corrupted checksum at byte 17
    auto bad18Cksm = valid18Text;
    bad18Cksm[17] ^= 0xFFU; // Corrupted checksum byte
    assert(!PelcoD::PelcoDFrame::isValidFrame(bad18Cksm));
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

    // Corrupted 7-byte frame must not be extracted as a false 4-byte frame
    const std::vector<std::uint8_t> corrupted7 { 0xFFU, 0x01U, 0x00U, 0x59U, 0x10U, 0x20U, 0x99U };
    const auto badFrames = PelcoD::PelcoDFrame::splitStream(corrupted7);
    assert(badFrames.empty());

    // Three consecutive 7-byte frames in a 21-byte stream must all be extracted as 7-byte frames
    std::vector<std::uint8_t> multiStream;
    const auto f1 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x59U, 0x10U, 0x20U);
    const auto f2 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5BU, 0x05U, 0x10U);
    const auto f3 = PelcoD::PelcoDFrame::createFrame(0x01U, 0x00U, 0x5DU, 0x04U, 0xB0U);
    multiStream.insert(multiStream.end(), f1.begin(), f1.end());
    multiStream.insert(multiStream.end(), f2.begin(), f2.end());
    multiStream.insert(multiStream.end(), f3.begin(), f3.end());
    assert(multiStream.size() == 21U);

    const auto multiFrames = PelcoD::PelcoDFrame::splitStream(multiStream);
    assert(multiFrames.size() == 3U);
    assert(multiFrames[0].size() == 7U);
    assert(multiFrames[1].size() == 7U);
    assert(multiFrames[2].size() == 7U);

    // Valid 18-byte query frame extraction via splitStream
    std::vector<std::uint8_t> qFrame(18U, 0x00U);
    qFrame[0] = 0xFFU;
    qFrame[1] = 0x01U;
    qFrame[2] = 'S';
    qFrame[3] = 'E';
    qFrame[4] = 'N';
    qFrame[5] = 'S';
    qFrame[6] = 'O';
    qFrame[7] = 'R';
    qFrame[17] = PelcoD::PelcoDFrame::calculateChecksum(&qFrame[1], 16U);
    const auto qFrames = PelcoD::PelcoDFrame::splitStream(qFrame);
    assert(qFrames.size() == 1U);
    assert(qFrames[0].size() == 18U);

    // Corrupted 18-byte query frame checksum must be rejected
    auto badQFrame = qFrame;
    badQFrame[17] ^= 0x55U;
    const auto badQFrames = PelcoD::PelcoDFrame::splitStream(badQFrame);
    assert(badQFrames.empty());
}

void testToHexString()
{
    const std::vector<std::uint8_t> frame { 0xFFU, 0x01U, 0x00U, 0x04U, 0x20U, 0x00U, 0x25U };

    // Default space delimiter
    assert(PelcoD::PelcoDFrame::toHexString(frame) == "FF 01 00 04 20 00 25");

    // Custom delimiter ':'
    assert(PelcoD::PelcoDFrame::toHexString(frame, ':') == "FF:01:00:04:20:00:25");

    // No delimiter '\0'
    assert(PelcoD::PelcoDFrame::toHexString(frame, '\0') == "FF010004200025");

    // Empty vector
    assert(PelcoD::PelcoDFrame::toHexString(std::vector<std::uint8_t> {}).empty());

    // Raw pointer overload
    assert(PelcoD::PelcoDFrame::toHexString(frame.data(), frame.size(), '-') == "FF-01-00-04-20-00-25");
    assert(PelcoD::PelcoDFrame::toHexString(nullptr, 0U).empty());
}

void testFromHexString()
{
    const std::vector<std::uint8_t> expected { 0xFFU, 0x01U, 0x00U, 0x04U, 0x20U, 0x00U, 0x25U };

    // Standard spaced hex
    assert(PelcoD::PelcoDFrame::fromHexString("FF 01 00 04 20 00 25") == expected);

    // Unspaced hex
    assert(PelcoD::PelcoDFrame::fromHexString("FF010004200025") == expected);

    // Lowercase hex
    assert(PelcoD::PelcoDFrame::fromHexString("ff 01 00 04 20 00 25") == expected);

    // Colon-delimited
    assert(PelcoD::PelcoDFrame::fromHexString("FF:01:00:04:20:00:25") == expected);

    // With 0x / 0X prefixes
    assert(PelcoD::PelcoDFrame::fromHexString("0xFF 0x01 0x00 0x04 0x20 0x00 0x25") == expected);
    assert(PelcoD::PelcoDFrame::fromHexString("0XFF 0X01 0X00 0X04 0X20 0X00 0X25") == expected);

    // Mixed spacing and leading/trailing whitespace
    assert(PelcoD::PelcoDFrame::fromHexString("  FF  01  00 04   20 00 25 \t\r\n ") == expected);

    // Empty and whitespace-only
    assert(PelcoD::PelcoDFrame::fromHexString("").empty());
    assert(PelcoD::PelcoDFrame::fromHexString("   ").empty());

    // Incomplete / odd trailing nibble
    const std::vector<std::uint8_t> expectedPrefix { 0xFFU, 0x01U };
    assert(PelcoD::PelcoDFrame::fromHexString("FF 01 A") == expectedPrefix);

    // Non-hex characters ignored
    assert(PelcoD::PelcoDFrame::fromHexString("ZZ FF -- 01 !!") == expectedPrefix);

    // Round-trip test
    const auto serialized = PelcoD::PelcoDFrame::toHexString(expected);
    assert(PelcoD::PelcoDFrame::fromHexString(serialized) == expected);
}

int main()
{
    std::cout << "[TestPelcoDFrame] Running tests..." << std::endl;
    testChecksumCalculation();
    testCreateFrame();
    testFrameValidation();
    testStreamSplitting();
    testToHexString();
    testFromHexString();
    std::cout << "[TestPelcoDFrame] All tests passed successfully." << std::endl;
    return 0;
}
