#include "StubAnalyticsAdapter.h"

#include <cstddef>

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Onvif {

namespace {

    // Point in polygon test using Ray-Casting algorithm
    [[nodiscard]] bool isPointInPolygon(const Point2D& pt, const std::vector<Point2D>& polygon)
    {
        if (polygon.size() < 3U) {
            return false;
        }
        bool inside { false };
        const double px { static_cast<double>(pt.x) };
        const double py { static_cast<double>(pt.y) };
        const std::size_t n { polygon.size() };
        for (std::size_t i = 0U, j = n - 1U; i < n; j = i++) {
            const double xi { static_cast<double>(polygon[i].x) };
            const double yi { static_cast<double>(polygon[i].y) };
            const double xj { static_cast<double>(polygon[j].x) };
            const double yj { static_cast<double>(polygon[j].y) };
            const bool intersect { ((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi + 1e-12) + xi) };
            if (intersect) {
                inside = !inside;
            }
        }
        return inside;
    }

    // Line segment crossing test
    // Returns 0: no cross, 1: crossed left-to-right, -1: crossed right-to-left
    [[nodiscard]] int checkLineCrossing(const Point2D& p1, const Point2D& p2, const Point2D& lA, const Point2D& lB)
    {
        const auto ccw = [](const Point2D& a, const Point2D& b, const Point2D& c) -> double {
            const double ax { static_cast<double>(a.x) };
            const double ay { static_cast<double>(a.y) };
            const double bx { static_cast<double>(b.x) };
            const double by { static_cast<double>(b.y) };
            const double cx { static_cast<double>(c.x) };
            const double cy { static_cast<double>(c.y) };
            return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
        };

        const double d1 { ccw(lA, lB, p1) };
        const double d2 { ccw(lA, lB, p2) };
        const double d3 { ccw(p1, p2, lA) };
        const double d4 { ccw(p1, p2, lB) };

        if (((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) && ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0))) {
            return (d1 > 0.0) ? 1 : -1;
        }
        return 0;
    }

} // namespace

StubAnalyticsAdapter::StubAnalyticsAdapter() = default;

void StubAnalyticsAdapter::setEventPublisher(EventCallback publisher)
{
    std::scoped_lock lock(m_mutex);
    m_eventPublisher = std::move(publisher);
}

// =========================================================================
// IAnalyticsHandler Implementation
// =========================================================================

std::vector<AnalyticsRuleDescription> StubAnalyticsAdapter::handleGetSupportedRules(const std::string& /*configToken*/)
{
    return {
        { "tt:LineDetector", { "Segment", "Direction", "Classes", "MinConfidence", "Enabled" } },
        { "tt:FieldDetector", { "Field", "Classes", "MinConfidence", "Enabled" } },
        { "tt:LoiteringDetector", { "Field", "DwellTime", "Classes", "MinConfidence", "Enabled" } },
        { "tt:CellMotionDetector", { "Sensitivity", "Enabled" } },
    };
}

std::vector<AnalyticsRule> StubAnalyticsAdapter::handleGetRules(const std::string& /*configToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_rules;
}

bool StubAnalyticsAdapter::handleCreateRules(
    const std::string& /*configToken*/, const std::vector<AnalyticsRule>& rules)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& r : rules) {
        m_rules.push_back(r);
    }
    return true;
}

bool StubAnalyticsAdapter::handleModifyRules(
    const std::string& /*configToken*/, const std::vector<AnalyticsRule>& rules)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& r : rules) {
        for (auto& existing : m_rules) {
            if (existing.name == r.name) {
                existing = r;
                break;
            }
        }
    }
    return true;
}

bool StubAnalyticsAdapter::handleDeleteRules(
    const std::string& /*configToken*/, const std::vector<std::string>& ruleNames)
{
    std::scoped_lock lock(m_mutex);
    m_rules.erase(std::remove_if(m_rules.begin(), m_rules.end(),
                      [&](const AnalyticsRule& r) {
                          return std::find(ruleNames.begin(), ruleNames.end(), r.name) != ruleNames.end();
                      }),
        m_rules.end());
    return true;
}

std::vector<AnalyticsModuleDescription> StubAnalyticsAdapter::handleGetSupportedModules(
    const std::string& /*configToken*/)
{
    return {
        { "tt:ObjectClassificationModule", { "Classes", "MinConfidence" } },
        { "tt:MotionDetectionModule", { "Sensitivity" } },
    };
}

std::vector<AnalyticsModule> StubAnalyticsAdapter::handleGetAnalyticsModules(const std::string& /*configToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_analyticsModules;
}

bool StubAnalyticsAdapter::handleCreateAnalyticsModules(
    const std::string& /*configToken*/, const std::vector<AnalyticsModule>& modules)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& m : modules) {
        m_analyticsModules.push_back(m);
    }
    return true;
}

bool StubAnalyticsAdapter::handleModifyAnalyticsModules(
    const std::string& /*configToken*/, const std::vector<AnalyticsModule>& modules)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& m : modules) {
        for (auto& existing : m_analyticsModules) {
            if (existing.name == m.name) {
                existing = m;
                break;
            }
        }
    }
    return true;
}

bool StubAnalyticsAdapter::handleDeleteAnalyticsModules(
    const std::string& /*configToken*/, const std::vector<std::string>& moduleNames)
{
    std::scoped_lock lock(m_mutex);
    m_analyticsModules.erase(std::remove_if(m_analyticsModules.begin(), m_analyticsModules.end(),
                                 [&](const AnalyticsModule& m) {
                                     return std::find(moduleNames.begin(), moduleNames.end(), m.name)
                                         != moduleNames.end();
                                 }),
        m_analyticsModules.end());
    return true;
}

// =========================================================================
// IMetadataHandler Implementation
// =========================================================================

std::vector<MetadataConfiguration> StubAnalyticsAdapter::handleGetMetadataConfigs()
{
    std::scoped_lock lock(m_mutex);
    return m_metadataConfigs;
}

std::optional<MetadataConfiguration> StubAnalyticsAdapter::handleGetMetadataConfig(const std::string& token)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& c : m_metadataConfigs) {
        if (c.token == token) {
            return c;
        }
    }
    return std::nullopt;
}

bool StubAnalyticsAdapter::handleSetMetadataConfig(const MetadataConfiguration& config)
{
    std::scoped_lock lock(m_mutex);
    for (auto& c : m_metadataConfigs) {
        if (c.token == config.token) {
            c = config;
            return true;
        }
    }
    return false;
}

MetadataConfigurationOptions StubAnalyticsAdapter::handleGetMetaConfigOptions(
    const std::string& /*configToken*/, const std::string& /*profileToken*/)
{
    MetadataConfigurationOptions opt;
    opt.ptzStatusFilterSupported = true;
    opt.ptzPositionFilterSupported = true;
    opt.geoFilterSupported = true;
    return opt;
}

MetadataStreamPayload StubAnalyticsAdapter::handleGetCurrentMetadata(const std::string& /*profileToken*/)
{
    std::scoped_lock lock(m_mutex);
    MetadataStreamPayload p;
    p.topic = "tns1:VideoAnalytics/Objects";
    p.utcTime = "2026-09-17T00:00:00Z";
    p.detectedObjects = m_detectedObjects;
    return p;
}

void StubAnalyticsAdapter::setDetectedObjects(std::vector<AnalyticsObject> objects)
{
    EventCallback publisher { };
    std::vector<OnvifEvent> eventsToPublish { };

    {
        std::scoped_lock lock(m_mutex);
        m_detectedObjects = std::move(objects);
        publisher = m_eventPublisher;
        evaluateRulesForFrame(eventsToPublish);
    }

    if (publisher) {
        for (const auto& ev : eventsToPublish) {
            publisher(ev);
        }
    }
}

void StubAnalyticsAdapter::addDetectedObject(const AnalyticsObject& object)
{
    EventCallback publisher { };
    std::vector<OnvifEvent> eventsToPublish { };

    {
        std::scoped_lock lock(m_mutex);
        m_detectedObjects.push_back(object);
        publisher = m_eventPublisher;
        evaluateRulesForFrame(eventsToPublish);
    }

    if (publisher) {
        for (const auto& ev : eventsToPublish) {
            publisher(ev);
        }
    }
}

void StubAnalyticsAdapter::clearDetectedObjects()
{
    std::scoped_lock lock(m_mutex);
    m_detectedObjects.clear();
}

void StubAnalyticsAdapter::evaluateRulesForObject(
    const AnalyticsObject& prevObj, const AnalyticsObject& currentObj, std::vector<OnvifEvent>& outEvents)
{
    const Point2D prevCenter { (prevObj.boundingBox.left + prevObj.boundingBox.right) * 0.5f,
        (prevObj.boundingBox.top + prevObj.boundingBox.bottom) * 0.5f };
    const Point2D currCenter { (currentObj.boundingBox.left + currentObj.boundingBox.right) * 0.5f,
        (currentObj.boundingBox.top + currentObj.boundingBox.bottom) * 0.5f };

    for (const auto& rule : m_rules) {
        if (!rule.enabled) {
            continue;
        }

        if (!rule.objectClasses.empty()) {
            const bool classMatch
                = std::find(rule.objectClasses.begin(), rule.objectClasses.end(), currentObj.className)
                != rule.objectClasses.end();
            if (!classMatch) {
                continue;
            }
        }
        if (rule.minConfidence > 0.0f && currentObj.confidence < rule.minConfidence) {
            continue;
        }

        // 1. Line Crossing (Tripwire)
        if (rule.type.find("Line") != std::string::npos) {
            const int cross = checkLineCrossing(prevCenter, currCenter, rule.lineStart, rule.lineEnd);
            bool crossed = false;
            if (rule.direction == "LeftToRight" && cross == 1) {
                crossed = true;
            } else if (rule.direction == "RightToLeft" && cross == -1) {
                crossed = true;
            } else if ((rule.direction == "Any" || rule.direction.empty()) && cross != 0) {
                crossed = true;
            }

            if (crossed) {
                OnvifEvent ev { };
                ev.topic = "tns1:RuleEngine/LineDetector/Crossed";
                ev.sourceName = "Rule";
                ev.sourceValue = rule.name;
                ev.dataName = "State";
                ev.dataValue = "true";
                outEvents.push_back(ev);
            }
        }
    }
}

void StubAnalyticsAdapter::evaluateRulesForFrame(std::vector<OnvifEvent>& outEvents)
{
    const auto now = std::chrono::steady_clock::now();

    for (const auto& obj : m_detectedObjects) {
        auto& track = m_objectTracks[obj.objectId];
        const bool isNew = (track.firstSeen.time_since_epoch().count() == 0);
        if (isNew) {
            track.firstSeen = now;
            track.lastSeen = now;
            track.lastObject = obj;
        } else {
            evaluateRulesForObject(track.lastObject, obj, outEvents);
            track.lastSeen = now;
            track.lastObject = obj;
        }

        const Point2D center { (obj.boundingBox.left + obj.boundingBox.right) * 0.5f,
            (obj.boundingBox.top + obj.boundingBox.bottom) * 0.5f };

        for (const auto& rule : m_rules) {
            if (!rule.enabled) {
                continue;
            }

            if (!rule.objectClasses.empty()) {
                const bool classMatch = std::find(rule.objectClasses.begin(), rule.objectClasses.end(), obj.className)
                    != rule.objectClasses.end();
                if (!classMatch) {
                    continue;
                }
            }
            if (rule.minConfidence > 0.0f && obj.confidence < rule.minConfidence) {
                continue;
            }

            // 2. Field Detector (Polygon Intrusion)
            if (rule.type.find("Field") != std::string::npos && rule.type.find("Loitering") == std::string::npos) {
                if (isPointInPolygon(center, rule.polygon)) {
                    if (!track.triggeredRules[rule.name]) {
                        track.triggeredRules[rule.name] = true;
                        OnvifEvent ev { };
                        ev.topic = "tns1:RuleEngine/FieldDetector/ObjectsInside";
                        ev.sourceName = "Rule";
                        ev.sourceValue = rule.name;
                        ev.dataName = "State";
                        ev.dataValue = "true";
                        outEvents.push_back(ev);
                    }
                } else {
                    track.triggeredRules[rule.name] = false;
                }
            }

            // 3. Loitering Detector (Polygon Intrusion + Dwell Time)
            if (rule.type.find("Loitering") != std::string::npos) {
                if (isPointInPolygon(center, rule.polygon)) {
                    const double dwellSec = std::chrono::duration<double>(now - track.firstSeen).count();
                    if (dwellSec >= rule.dwellTimeSeconds && !track.triggeredRules[rule.name]) {
                        track.triggeredRules[rule.name] = true;
                        OnvifEvent ev { };
                        ev.topic = "tns1:RuleEngine/LoiteringDetector/DwellTimeExceeded";
                        ev.sourceName = "Rule";
                        ev.sourceValue = rule.name;
                        ev.dataName = "State";
                        ev.dataValue = "true";
                        outEvents.push_back(ev);
                    }
                } else {
                    track.firstSeen = now;
                    track.triggeredRules[rule.name] = false;
                }
            }
        }
    }
}

} // namespace Onvif
