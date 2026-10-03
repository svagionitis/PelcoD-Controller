#include "MdArray.h"
#include "KlvBer.h"
#include "Misb1201.h"

#include <algorithm>
#include <cstring>

namespace Klv {

namespace {

[[nodiscard]] double readFloat64(const std::uint8_t* p) noexcept {
    std::uint64_t bits { 0ULL };
    for (std::size_t i = 0U; i < 8U; ++i) {
        bits = (bits << 8U) | static_cast<std::uint64_t>(p[i]);
    }
    double val { 0.0 };
    std::memcpy(&val, &bits, sizeof(double));
    return val;
}

void writeFloat64(double val, std::vector<std::uint8_t>& out) {
    std::uint64_t bits { 0ULL };
    std::memcpy(&bits, &val, sizeof(double));
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((bits >> (i * 8)) & 0xFFU));
    }
}

[[nodiscard]] double readFloat32(const std::uint8_t* p) noexcept {
    std::uint32_t bits { 0U };
    for (std::size_t i = 0U; i < 4U; ++i) {
        bits = (bits << 8U) | static_cast<std::uint32_t>(p[i]);
    }
    float val { 0.0f };
    std::memcpy(&val, &bits, sizeof(float));
    return static_cast<double>(val);
}

[[nodiscard]] std::uint32_t readUInt(const std::uint8_t* p, std::size_t bytes) noexcept {
    std::uint32_t val { 0U };
    for (std::size_t i = 0U; i < bytes; ++i) {
        val = (val << 8U) | static_cast<std::uint32_t>(p[i]);
    }
    return val;
}

void writeUInt(std::uint32_t val, std::size_t bytes, std::vector<std::uint8_t>& out) {
    for (int i = static_cast<int>(bytes) - 1; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((val >> (i * 8)) & 0xFFU));
    }
}

} // namespace

bool MdArray::encodeFloat2D(const std::vector<std::vector<double>>& matrix,
                            double minVal,
                            double maxVal,
                            std::size_t ebytes,
                            std::vector<std::uint8_t>& out) {
    if (matrix.empty() || matrix[0].empty() || minVal >= maxVal || ebytes == 0U || ebytes > 8U) {
        return false;
    }
    const std::size_t numRows = matrix.size();
    const std::size_t numCols = matrix[0].size();
    for (const auto& row : matrix) {
        if (row.size() != numCols) {
            return false;
        }
    }

    KlvBer::encodeTag(2U, out); // ndim = 2
    KlvBer::encodeTag(static_cast<std::uint32_t>(numRows), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(numCols), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(ebytes), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(MdArrayApa::ST1201), out);

    // APAS values: min and max as 64-bit IEEE float
    writeFloat64(minVal, out);
    writeFloat64(maxVal, out);

    for (std::size_t r = 0U; r < numRows; ++r) {
        for (std::size_t c = 0U; c < numCols; ++c) {
            Misb1201::encodeBytes(matrix[r][c], minVal, maxVal, ebytes, out);
        }
    }
    return true;
}

bool MdArray::encodeFloat1D(const std::vector<double>& vec,
                            double minVal,
                            double maxVal,
                            std::size_t ebytes,
                            std::vector<std::uint8_t>& out) {
    if (vec.empty() || minVal >= maxVal || ebytes == 0U || ebytes > 8U) {
        return false;
    }

    KlvBer::encodeTag(1U, out); // ndim = 1
    KlvBer::encodeTag(static_cast<std::uint32_t>(vec.size()), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(ebytes), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(MdArrayApa::ST1201), out);

    writeFloat64(minVal, out);
    writeFloat64(maxVal, out);

    for (const double val : vec) {
        Misb1201::encodeBytes(val, minVal, maxVal, ebytes, out);
    }
    return true;
}

bool MdArray::encodeUInt2D(const std::vector<std::vector<std::uint32_t>>& matrix,
                           std::size_t ebytes,
                           std::vector<std::uint8_t>& out) {
    if (matrix.empty() || matrix[0].empty() || (ebytes != 1U && ebytes != 2U && ebytes != 4U)) {
        return false;
    }
    const std::size_t numRows = matrix.size();
    const std::size_t numCols = matrix[0].size();
    for (const auto& row : matrix) {
        if (row.size() != numCols) {
            return false;
        }
    }

    KlvBer::encodeTag(2U, out); // ndim = 2
    KlvBer::encodeTag(static_cast<std::uint32_t>(numRows), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(numCols), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(ebytes), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(MdArrayApa::NaturalFormat), out);

    for (std::size_t r = 0U; r < numRows; ++r) {
        for (std::size_t c = 0U; c < numCols; ++c) {
            writeUInt(matrix[r][c], ebytes, out);
        }
    }
    return true;
}

bool MdArray::encodeUInt1D(const std::vector<std::uint32_t>& vec,
                           std::size_t ebytes,
                           std::vector<std::uint8_t>& out) {
    if (vec.empty() || (ebytes != 1U && ebytes != 2U && ebytes != 4U)) {
        return false;
    }

    KlvBer::encodeTag(1U, out); // ndim = 1
    KlvBer::encodeTag(static_cast<std::uint32_t>(vec.size()), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(ebytes), out);
    KlvBer::encodeTag(static_cast<std::uint32_t>(MdArrayApa::NaturalFormat), out);

    for (const std::uint32_t val : vec) {
        writeUInt(val, ebytes, out);
    }
    return true;
}

bool MdArray::decodeFloat2D(const std::uint8_t* data,
                            std::size_t size,
                            std::vector<std::vector<double>>& outMatrix) {
    outMatrix.clear();
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::size_t offset { 0U };
    std::uint32_t ndim { 0U };
    std::size_t consumed { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, ndim, consumed) || ndim != 2U) {
        return false;
    }
    offset += consumed;

    std::uint32_t dim1 { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, dim1, consumed) || dim1 == 0U) {
        return false;
    }
    offset += consumed;

    std::uint32_t dim2 { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, dim2, consumed) || dim2 == 0U) {
        return false;
    }
    offset += consumed;

    std::uint32_t ebytes { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, ebytes, consumed) || ebytes == 0U || ebytes > 8U) {
        return false;
    }
    offset += consumed;

    std::uint32_t apa { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, apa, consumed)) {
        return false;
    }
    offset += consumed;

    const auto numRows = static_cast<std::size_t>(dim1);
    const auto numCols = static_cast<std::size_t>(dim2);
    const std::size_t totalElements = numRows * numCols;
    const std::size_t elementDataLen = totalElements * static_cast<std::size_t>(ebytes);

    if (apa == static_cast<std::uint32_t>(MdArrayApa::ST1201)) {
        if (offset + elementDataLen > size) {
            return false;
        }
        const std::size_t remaining = size - offset;
        if (remaining < elementDataLen) {
            return false;
        }
        const std::size_t apasLen = remaining - elementDataLen;
        double minVal { 0.0 };
        double maxVal { 0.0 };

        if (apasLen == 16U) {
            minVal = readFloat64(data + offset);
            maxVal = readFloat64(data + offset + 8U);
            offset += 16U;
        } else if (apasLen == 8U) {
            minVal = readFloat32(data + offset);
            maxVal = readFloat32(data + offset + 4U);
            offset += 8U;
        } else {
            return false;
        }

        outMatrix.assign(numRows, std::vector<double>(numCols, 0.0));
        for (std::size_t r = 0U; r < numRows; ++r) {
            for (std::size_t c = 0U; c < numCols; ++c) {
                const auto res = Misb1201::decodeBytes(data + offset, ebytes, minVal, maxVal);
                outMatrix[r][c] = res.value;
                offset += ebytes;
            }
        }
        return true;
    }

    if (apa == static_cast<std::uint32_t>(MdArrayApa::NaturalFormat)) {
        if (offset + elementDataLen > size) {
            return false;
        }
        outMatrix.assign(numRows, std::vector<double>(numCols, 0.0));
        for (std::size_t r = 0U; r < numRows; ++r) {
            for (std::size_t c = 0U; c < numCols; ++c) {
                if (ebytes == 8U) {
                    outMatrix[r][c] = readFloat64(data + offset);
                } else if (ebytes == 4U) {
                    outMatrix[r][c] = readFloat32(data + offset);
                } else {
                    return false;
                }
                offset += ebytes;
            }
        }
        return true;
    }

    return false;
}

bool MdArray::decodeFloat1D(const std::uint8_t* data,
                            std::size_t size,
                            std::vector<double>& outVec) {
    outVec.clear();
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::size_t offset { 0U };
    std::uint32_t ndim { 0U };
    std::size_t consumed { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, ndim, consumed) || ndim != 1U) {
        return false;
    }
    offset += consumed;

    std::uint32_t dim1 { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, dim1, consumed) || dim1 == 0U) {
        return false;
    }
    offset += consumed;

    std::uint32_t ebytes { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, ebytes, consumed) || ebytes == 0U || ebytes > 8U) {
        return false;
    }
    offset += consumed;

    std::uint32_t apa { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, apa, consumed)) {
        return false;
    }
    offset += consumed;

    const auto numElements = static_cast<std::size_t>(dim1);
    const std::size_t elementDataLen = numElements * static_cast<std::size_t>(ebytes);

    if (apa == static_cast<std::uint32_t>(MdArrayApa::ST1201)) {
        if (offset + elementDataLen > size) {
            return false;
        }
        const std::size_t remaining = size - offset;
        if (remaining < elementDataLen) {
            return false;
        }
        const std::size_t apasLen = remaining - elementDataLen;
        double minVal { 0.0 };
        double maxVal { 0.0 };

        if (apasLen == 16U) {
            minVal = readFloat64(data + offset);
            maxVal = readFloat64(data + offset + 8U);
            offset += 16U;
        } else if (apasLen == 8U) {
            minVal = readFloat32(data + offset);
            maxVal = readFloat32(data + offset + 4U);
            offset += 8U;
        } else {
            return false;
        }

        outVec.resize(numElements, 0.0);
        for (std::size_t i = 0U; i < numElements; ++i) {
            const auto res = Misb1201::decodeBytes(data + offset, ebytes, minVal, maxVal);
            outVec[i] = res.value;
            offset += ebytes;
        }
        return true;
    }

    if (apa == static_cast<std::uint32_t>(MdArrayApa::NaturalFormat)) {
        if (offset + elementDataLen > size) {
            return false;
        }
        outVec.resize(numElements, 0.0);
        for (std::size_t i = 0U; i < numElements; ++i) {
            if (ebytes == 8U) {
                outVec[i] = readFloat64(data + offset);
            } else if (ebytes == 4U) {
                outVec[i] = readFloat32(data + offset);
            } else {
                return false;
            }
            offset += ebytes;
        }
        return true;
    }

    return false;
}

bool MdArray::decodeUInt2D(const std::uint8_t* data,
                           std::size_t size,
                           std::vector<std::vector<std::uint32_t>>& outMatrix) {
    outMatrix.clear();
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::size_t offset { 0U };
    std::uint32_t ndim { 0U };
    std::size_t consumed { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, ndim, consumed) || ndim != 2U) {
        return false;
    }
    offset += consumed;

    std::uint32_t dim1 { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, dim1, consumed) || dim1 == 0U) {
        return false;
    }
    offset += consumed;

    std::uint32_t dim2 { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, dim2, consumed) || dim2 == 0U) {
        return false;
    }
    offset += consumed;

    std::uint32_t ebytes { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, ebytes, consumed) || ebytes == 0U) {
        return false;
    }
    offset += consumed;

    std::uint32_t apa { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, apa, consumed)) {
        return false;
    }
    offset += consumed;

    const auto numRows = static_cast<std::size_t>(dim1);
    const auto numCols = static_cast<std::size_t>(dim2);

    if (apa == static_cast<std::uint32_t>(MdArrayApa::NaturalFormat)) {
        const std::size_t elementDataLen = numRows * numCols * static_cast<std::size_t>(ebytes);
        if (offset + elementDataLen > size) {
            return false;
        }
        outMatrix.assign(numRows, std::vector<std::uint32_t>(numCols, 0U));
        for (std::size_t r = 0U; r < numRows; ++r) {
            for (std::size_t c = 0U; c < numCols; ++c) {
                outMatrix[r][c] = readUInt(data + offset, ebytes);
                offset += ebytes;
            }
        }
        return true;
    }

    if (apa == static_cast<std::uint32_t>(MdArrayApa::UnsignedInteger)) {
        std::uint32_t bias { 0U };
        if (!KlvBer::decodeTag(data + offset, size - offset, bias, consumed)) {
            return false;
        }
        offset += consumed;

        outMatrix.assign(numRows, std::vector<std::uint32_t>(numCols, 0U));
        for (std::size_t r = 0U; r < numRows; ++r) {
            for (std::size_t c = 0U; c < numCols; ++c) {
                std::uint32_t relVal { 0U };
                if (!KlvBer::decodeTag(data + offset, size - offset, relVal, consumed)) {
                    return false;
                }
                offset += consumed;
                outMatrix[r][c] = bias + relVal;
            }
        }
        return true;
    }

    return false;
}

bool MdArray::decodeUInt1D(const std::uint8_t* data,
                           std::size_t size,
                           std::vector<std::uint32_t>& outVec) {
    outVec.clear();
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::size_t offset { 0U };
    std::uint32_t ndim { 0U };
    std::size_t consumed { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, ndim, consumed) || ndim != 1U) {
        return false;
    }
    offset += consumed;

    std::uint32_t dim1 { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, dim1, consumed) || dim1 == 0U) {
        return false;
    }
    offset += consumed;

    std::uint32_t ebytes { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, ebytes, consumed) || ebytes == 0U) {
        return false;
    }
    offset += consumed;

    std::uint32_t apa { 0U };
    if (!KlvBer::decodeTag(data + offset, size - offset, apa, consumed)) {
        return false;
    }
    offset += consumed;

    const auto numElements = static_cast<std::size_t>(dim1);

    if (apa == static_cast<std::uint32_t>(MdArrayApa::NaturalFormat)) {
        const std::size_t elementDataLen = numElements * static_cast<std::size_t>(ebytes);
        if (offset + elementDataLen > size) {
            return false;
        }
        outVec.resize(numElements, 0U);
        for (std::size_t i = 0U; i < numElements; ++i) {
            outVec[i] = readUInt(data + offset, ebytes);
            offset += ebytes;
        }
        return true;
    }

    if (apa == static_cast<std::uint32_t>(MdArrayApa::UnsignedInteger)) {
        std::uint32_t bias { 0U };
        if (!KlvBer::decodeTag(data + offset, size - offset, bias, consumed)) {
            return false;
        }
        offset += consumed;

        outVec.resize(numElements, 0U);
        for (std::size_t i = 0U; i < numElements; ++i) {
            std::uint32_t relVal { 0U };
            if (!KlvBer::decodeTag(data + offset, size - offset, relVal, consumed)) {
                return false;
            }
            offset += consumed;
            outVec[i] = bias + relVal;
        }
        return true;
    }

    return false;
}

} // namespace Klv
