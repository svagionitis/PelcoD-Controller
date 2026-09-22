#pragma once

/// @file QViscaBusScanner.h
/// @brief Qt wrapper providing signals and asynchronous bus discovery for ViscaBusScanner.

#include <Transport/ITransport.h>
#include <ViscaBusScanner.h>

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <vector>

namespace ViscaApp {

/// @class QViscaBusScanner
/// @brief Manages asynchronous daisy-chain VISCA bus scanning across addresses 1..7.
class QViscaBusScanner : public QObject {
    Q_OBJECT

public:
    explicit QViscaBusScanner(std::shared_ptr<::Transport::ITransport> transport = nullptr, QObject* parent = nullptr);
    ~QViscaBusScanner() override = default;

    // Non-copyable, non-movable
    QViscaBusScanner(const QViscaBusScanner&) = delete;
    QViscaBusScanner& operator=(const QViscaBusScanner&) = delete;

    [[nodiscard]] bool isScanning() const noexcept
    {
        return m_scanning.load();
    }

    void setTransport(std::shared_ptr<::Transport::ITransport> transport);
    [[nodiscard]] std::shared_ptr<::Transport::ITransport> transport() const noexcept;

signals:
    void scanStarted();
    void scanProgress(int current, int total, int percent);
    void deviceDiscovered(const Visca::DiscoveredCamera& camera);
    void scanFinished(int totalFound);
    void scanFailed(const QString& error);

public slots:
    bool startScan();
    void cancelScan();

private:
    std::shared_ptr<::Transport::ITransport> m_transport;
    std::atomic<bool> m_scanning { false };
    std::atomic<bool> m_cancelRequested { false };
};

} // namespace ViscaApp
