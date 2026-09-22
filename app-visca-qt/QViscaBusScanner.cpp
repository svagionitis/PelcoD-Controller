/// @file QViscaBusScanner.cpp
/// @brief Implementation of asynchronous Qt VISCA bus scanner.

#include "QViscaBusScanner.h"

#include <QMetaObject>
#include <QThreadPool>

namespace ViscaApp {

QViscaBusScanner::QViscaBusScanner(std::shared_ptr<::Transport::ITransport> transport, QObject* parent)
    : QObject(parent)
    , m_transport(std::move(transport))
{
}

void QViscaBusScanner::setTransport(std::shared_ptr<::Transport::ITransport> transport)
{
    m_transport = std::move(transport);
}

std::shared_ptr<::Transport::ITransport> QViscaBusScanner::transport() const noexcept
{
    return m_transport;
}

bool QViscaBusScanner::startScan()
{
    if (m_scanning.load()) {
        return false;
    }

    if (!m_transport) {
        emit scanFailed(tr("No transport configured for bus scanner."));
        return false;
    }

    if (!m_transport->isOpen()) {
        if (!m_transport->open()) {
            emit scanFailed(tr("Failed to open transport interface for scanning."));
            return false;
        }
    }

    m_scanning.store(true);
    m_cancelRequested.store(false);
    emit scanStarted();

    auto trans = m_transport;

    QThreadPool::globalInstance()->start([this, trans]() {
        Visca::ViscaBusScanner scanner(trans);

        auto progressCb = [this](size_t current, size_t total) {
            const int percent = total > 0 ? static_cast<int>((current * 100) / total) : 0;
            QMetaObject::invokeMethod(
                this,
                [this, current, total, percent]() {
                    emit scanProgress(static_cast<int>(current), static_cast<int>(total), percent);
                },
                Qt::QueuedConnection);
        };

        auto foundCb = [this](const Visca::DiscoveredCamera& cam) {
            QMetaObject::invokeMethod(
                this, [this, cam]() { emit deviceDiscovered(cam); }, Qt::QueuedConnection);
        };

        bool fatalError = false;
        auto errorCb = [this, &fatalError](Visca::ScanError err, const std::string& msg) {
            if (err == Visca::ScanError::TransportNotOpen || err == Visca::ScanError::AddressSetSendFailed) {
                fatalError = true;
                QMetaObject::invokeMethod(
                    this,
                    [this, msg]() {
                        m_scanning.store(false);
                        emit scanFailed(QString::fromStdString(msg));
                    },
                    Qt::QueuedConnection);
            }
        };

        auto isCancelled = [this]() -> bool {
            return m_cancelRequested.load();
        };

        const auto found = scanner.scanBus(progressCb, foundCb, errorCb, isCancelled);
        const int totalCount = static_cast<int>(found.size());

        if (!fatalError) {
            QMetaObject::invokeMethod(
                this,
                [this, totalCount]() {
                    m_scanning.store(false);
                    emit scanFinished(totalCount);
                },
                Qt::QueuedConnection);
        }
    });

    return true;
}

void QViscaBusScanner::cancelScan()
{
    m_cancelRequested.store(true);
}

} // namespace ViscaApp
