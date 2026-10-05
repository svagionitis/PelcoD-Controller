#include "SdpGenerator.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

namespace Transport {

namespace {

    constexpr std::string_view kCrLf = "\r\n";

} // namespace

bool SdpGenerator::isMulticast(std::string_view ip) noexcept
{
    const std::size_t dotPos = ip.find('.');
    if (dotPos == std::string_view::npos) {
        return false;
    }
    const std::string firstOctetStr(ip.substr(0U, dotPos));
    try {
        const int firstOctet = std::stoi(firstOctetStr);
        return firstOctet >= 224 && firstOctet <= 239;
    } catch (...) {
        return false;
    }
}

std::string SdpGenerator::generate(const SdpSessionParams& params)
{
    std::ostringstream ss {};

    // 1. Protocol Version (v=)
    ss << "v=0" << kCrLf;

    // 2. Origin (o=)
    const std::uint64_t sessId = (params.sessionId > 0U)
        ? params.sessionId
        : static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
    const std::string origin = params.originAddress.empty() ? "127.0.0.1" : params.originAddress;
    ss << "o=- " << sessId << " 1 IN IP4 " << origin << kCrLf;

    // 3. Session Name (s=)
    const std::string name = params.sessionName.empty() ? "Sightline SLA Video Stream" : params.sessionName;
    ss << "s=" << name << kCrLf;

    // 4. Connection Data (c=)
    const std::string dest = params.destinationIp.empty() ? "127.0.0.1" : params.destinationIp;
    const bool mcast = params.isMulticast || isMulticast(dest);
    ss << "c=IN IP4 " << dest;
    if (mcast) {
        ss << "/" << static_cast<int>(params.ttl > 0U ? params.ttl : 16U);
    }
    ss << kCrLf;

    // 5. Bandwidth (b=) [Optional]
    if (params.bitrateKbps > 0) {
        ss << "b=AS:" << params.bitrateKbps << kCrLf;
    }

    // 6. Timing (t=) [Live stream = 0 0]
    ss << "t=0 0" << kCrLf;

    // 7. Media Description (m=) & Attributes (a=)
    const std::uint16_t port = params.destinationPort > 0U ? params.destinationPort : 15004U;

    if (params.transportProtocol == SdpProtocol::Udp) {
        // Direct UDP stream (MPEG2-TS)
        ss << "m=video " << port << " udp 33" << kCrLf;
        ss << "a=rtpmap:33 MP2T/90000" << kCrLf;
    } else {
        // RTP/AVP streams
        switch (params.payloadType) {
        case SdpPayloadType::H264: {
            ss << "m=video " << port << " RTP/AVP 96" << kCrLf;
            ss << "a=rtpmap:96 H264/" << (params.clockRate > 0U ? params.clockRate : 90000U) << kCrLf;
            ss << "a=fmtp:96 packetization-mode=1";
            if (!params.spropParams.empty()) {
                ss << ";sprop-parameter-sets=" << params.spropParams;
            }
            ss << kCrLf;
            break;
        }
        case SdpPayloadType::H265: {
            ss << "m=video " << port << " RTP/AVP 97" << kCrLf;
            ss << "a=rtpmap:97 H265/" << (params.clockRate > 0U ? params.clockRate : 90000U) << kCrLf;
            ss << "a=fmtp:97 packetization-mode=1" << kCrLf;
            break;
        }
        case SdpPayloadType::Mjpeg: {
            ss << "m=video " << port << " RTP/AVP 26" << kCrLf;
            ss << "a=rtpmap:26 JPEG/90000" << kCrLf;
            break;
        }
        case SdpPayloadType::Mpeg2Ts:
        case SdpPayloadType::Custom:
        default: {
            ss << "m=video " << port << " RTP/AVP 33" << kCrLf;
            ss << "a=rtpmap:33 MP2T/90000" << kCrLf;
            break;
        }
        }
    }

    // Direction attribute
    ss << "a=recvonly" << kCrLf;

    // Optional frame dimensions
    if (params.frameWidth > 0 && params.frameHeight > 0) {
        ss << "a=x-dimensions:" << params.frameWidth << "," << params.frameHeight << kCrLf;
    }

    return ss.str();
}

bool SdpGenerator::saveToFile(std::string_view path, const SdpSessionParams& params)
{
    const std::string sdpContent = generate(params);
    const std::string filename(path);

    std::ofstream out(filename, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out.write(sdpContent.data(), static_cast<std::streamsize>(sdpContent.size()));
    return out.good();
}

SdpSessionParams SdpGenerator::parse(std::string_view sdpContent)
{
    SdpSessionParams params {};
    std::size_t startPos { 0U };

    while (startPos < sdpContent.size()) {
        std::size_t endPos = sdpContent.find('\n', startPos);
        if (endPos == std::string_view::npos) {
            endPos = sdpContent.size();
        }

        std::string_view line = sdpContent.substr(startPos, endPos - startPos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }

        if (line.size() >= 2U && line[1] == '=') {
            const char type = line[0];
            const std::string_view val = line.substr(2U);

            switch (type) {
            case 's':
                params.sessionName = std::string(val);
                break;
            case 'o': {
                std::istringstream oss { std::string(val) };
                std::string dash {};
                std::string sessIdStr {};
                std::string ver {};
                std::string inStr {};
                std::string ip4Str {};
                std::string originStr {};
                if (oss >> dash >> sessIdStr >> ver >> inStr >> ip4Str >> originStr) {
                    try {
                        params.sessionId = std::stoull(sessIdStr);
                    } catch (...) {
                    }
                    params.originAddress = originStr;
                }
                break;
            }
            case 'c': {
                // c=IN IP4 <ip>[/<ttl>]
                const std::size_t ip4Pos = val.find("IP4 ");
                if (ip4Pos != std::string_view::npos) {
                    std::string_view ipPart = val.substr(ip4Pos + 4U);
                    const std::size_t slashPos = ipPart.find('/');
                    if (slashPos != std::string_view::npos) {
                        params.destinationIp = std::string(ipPart.substr(0U, slashPos));
                        try {
                            params.ttl
                                = static_cast<std::uint8_t>(std::stoi(std::string(ipPart.substr(slashPos + 1U))));
                        } catch (...) {
                        }
                    } else {
                        params.destinationIp = std::string(ipPart);
                    }
                    params.isMulticast = isMulticast(params.destinationIp);
                }
                break;
            }
            case 'b': {
                // b=AS:<bitrate>
                if (val.rfind("AS:", 0) == 0) {
                    try {
                        params.bitrateKbps = std::stoi(std::string(val.substr(3U)));
                    } catch (...) {
                    }
                }
                break;
            }
            case 'm': {
                std::istringstream mss { std::string(val) };
                std::string med {};
                int port { 0 };
                std::string proto {};
                int pt { 0 };
                if (mss >> med >> port >> proto >> pt) {
                    params.destinationPort = static_cast<std::uint16_t>(port);
                    if (proto == "udp") {
                        params.transportProtocol = SdpProtocol::Udp;
                    } else {
                        params.transportProtocol = SdpProtocol::RtpAvp;
                    }

                    if (pt == 96) {
                        params.payloadType = SdpPayloadType::H264;
                    } else if (pt == 97) {
                        params.payloadType = SdpPayloadType::H265;
                    } else if (pt == 26) {
                        params.payloadType = SdpPayloadType::Mjpeg;
                    } else {
                        params.payloadType = SdpPayloadType::Mpeg2Ts;
                    }
                }
                break;
            }
            case 'a': {
                if (val.rfind("rtpmap:", 0) == 0) {
                    const std::size_t slashPos = val.find('/');
                    if (slashPos != std::string_view::npos) {
                        try {
                            params.clockRate
                                = static_cast<std::uint32_t>(std::stoul(std::string(val.substr(slashPos + 1U))));
                        } catch (...) {
                        }
                    }
                    if (val.find("H264") != std::string_view::npos) {
                        params.payloadType = SdpPayloadType::H264;
                    } else if (val.find("H265") != std::string_view::npos) {
                        params.payloadType = SdpPayloadType::H265;
                    } else if (val.find("JPEG") != std::string_view::npos) {
                        params.payloadType = SdpPayloadType::Mjpeg;
                    } else if (val.find("MP2T") != std::string_view::npos) {
                        params.payloadType = SdpPayloadType::Mpeg2Ts;
                    }
                } else if (val.rfind("x-dimensions:", 0) == 0) {
                    const std::string_view dim = val.substr(13U);
                    const std::size_t commaPos = dim.find(',');
                    if (commaPos != std::string_view::npos) {
                        try {
                            params.frameWidth = std::stoi(std::string(dim.substr(0U, commaPos)));
                            params.frameHeight = std::stoi(std::string(dim.substr(commaPos + 1U)));
                        } catch (...) {
                        }
                    }
                }
                break;
            }
            default:
                break;
            }
        }

        startPos = endPos + 1U;
    }

    return params;
}

} // namespace Transport
