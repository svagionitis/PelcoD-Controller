/// @file SightlineCrc8.cpp
/// @brief Implementation of Sightline CRC-8 lookup table and calculation.

#include "SightlineCrc8.h"

namespace Sightline {

const std::array<std::uint8_t, 256> SightlineCrc8::Table = { 0U, 94U, 188U, 226U, 97U, 63U, 221U, 131U, 194U, 156U,
    126U, 32U, 163U, 253U, 31U, 65U, 157U, 195U, 33U, 127U, 252U, 162U, 64U, 30U, 95U, 1U, 227U, 189U, 62U, 96U, 130U,
    220U, 35U, 125U, 159U, 193U, 66U, 28U, 254U, 160U, 225U, 191U, 93U, 3U, 128U, 222U, 60U, 98U, 190U, 224U, 2U, 92U,
    223U, 129U, 99U, 61U, 124U, 34U, 192U, 158U, 29U, 67U, 161U, 255U, 70U, 24U, 250U, 164U, 39U, 121U, 155U, 197U,
    132U, 218U, 56U, 102U, 229U, 187U, 89U, 7U, 219U, 133U, 103U, 57U, 186U, 228U, 6U, 88U, 25U, 71U, 165U, 251U, 120U,
    38U, 196U, 154U, 101U, 59U, 217U, 135U, 4U, 90U, 184U, 230U, 167U, 249U, 27U, 69U, 198U, 152U, 122U, 36U, 248U,
    166U, 68U, 26U, 153U, 199U, 37U, 123U, 58U, 100U, 134U, 216U, 91U, 5U, 231U, 185U, 140U, 210U, 48U, 110U, 237U,
    179U, 81U, 15U, 78U, 16U, 242U, 172U, 47U, 113U, 147U, 205U, 17U, 79U, 173U, 243U, 112U, 46U, 204U, 146U, 211U,
    141U, 111U, 49U, 178U, 236U, 14U, 80U, 175U, 241U, 19U, 77U, 206U, 144U, 114U, 44U, 109U, 51U, 209U, 143U, 12U, 82U,
    176U, 238U, 50U, 108U, 142U, 208U, 83U, 13U, 239U, 177U, 240U, 174U, 76U, 18U, 145U, 207U, 45U, 115U, 202U, 148U,
    118U, 40U, 171U, 245U, 23U, 73U, 8U, 86U, 180U, 234U, 105U, 55U, 213U, 139U, 87U, 9U, 235U, 181U, 54U, 104U, 138U,
    212U, 149U, 203U, 41U, 119U, 244U, 170U, 72U, 22U, 233U, 183U, 85U, 11U, 136U, 214U, 52U, 106U, 43U, 117U, 151U,
    201U, 74U, 20U, 246U, 168U, 116U, 42U, 200U, 150U, 21U, 75U, 169U, 247U, 182U, 232U, 10U, 84U, 215U, 137U, 107U,
    53U };

std::uint8_t SightlineCrc8::compute(const std::uint8_t* data, std::size_t size, std::uint8_t seed) noexcept
{
    if (data == nullptr || size == 0U) {
        return seed;
    }

    std::uint8_t crc { seed };
    for (std::size_t i = 0U; i < size; ++i) {
        crc = Table[static_cast<std::size_t>(crc ^ data[i])];
    }
    return crc;
}

bool SightlineCrc8::validate(const std::uint8_t* data, std::size_t size, std::uint8_t expectedChecksum) noexcept
{
    return compute(data, size, InitialSeed) == expectedChecksum;
}

} // namespace Sightline
