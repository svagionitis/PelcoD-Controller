#pragma once

/// @file EventService.h
/// @brief Modular ONVIF Event Service (tev) and WS-BaseNotification dispatcher.

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "IService.h"
#include "OnvifServerTypes.h"
#include "OnvifTypes.h"
#include "SoapRouter.h"

namespace Onvif {

class SubscriptionManager;
class NotificationDispatcher;

/// @struct EventServiceContext
/// @brief External port handlers and environment injected into EventService.
struct EventServiceContext {
    OnvifServerConfig config { };
    std::shared_ptr<SubscriptionManager> subManager { };
    std::shared_ptr<NotificationDispatcher> dispatcher { };
    const std::atomic<bool>* runningFlag { nullptr };
    std::function<std::string(const SoapContext&)> resolveHost { };
};

/// @class EventService
/// @brief Modular service implementation for ONVIF Events (tev and WS-BaseNotification).
class EventService : public IService {
public:
    explicit EventService(EventServiceContext ctx);
    ~EventService() override = default;

    EventService(const EventService&) = delete;
    EventService& operator=(const EventService&) = delete;
    EventService(EventService&&) = default;
    EventService& operator=(EventService&&) = default;

    void registerRoutes(SoapRouter& router) override;
    [[nodiscard]] std::string_view serviceName() const noexcept override;

private:
    SoapHandlerResult handleGetProperties(const SoapContext& ctx);
    SoapHandlerResult handleCreatePullPoint(const SoapContext& ctx);
    SoapHandlerResult handlePullMessages(const SoapContext& ctx);
    SoapHandlerResult handleRenew(const SoapContext& ctx);
    SoapHandlerResult handleUnsubscribe(const SoapContext& ctx);
    SoapHandlerResult handleSubscribe(const SoapContext& ctx);
    SoapHandlerResult handleSeek(const SoapContext& ctx);

    EventServiceContext m_ctx;
};

} // namespace Onvif
