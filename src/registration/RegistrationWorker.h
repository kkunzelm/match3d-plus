#pragma once

#include "../io/ViffReader.h"
#include "../RoiMask.h"
#include "Transformation3D.h"

#include <GenericProgressCallback.h>
#include <PointCloud.h>

#include <QObject>
#include <QString>
#include <atomic>

// Runs ICP registration on a worker thread.
// Create, moveToThread(), connect signals, then call run() via thread->start().
class RegistrationWorker : public QObject, public CCCoreLib::GenericProgressCallback {
    Q_OBJECT

public:
    struct Config {
        ViffImage modelImg;   // reference cloud (will not move)
        RoiMask   modelRoi;   // empty = use all valid pixels
        ViffImage dataImg;    // data cloud (will be aligned to model)
        RoiMask   dataRoi;    // empty = use all valid pixels
        Transformation3D initialTransform;  // coarse transform already computed
        // Registration parameters
        int    maxIterations  = 8000;
        int    samplingLimit  = 50000;
        double overlapRatio   = 0.95;   // 1.0 = full overlap expected
        double minRMSDecrease = 1.0e-5;
        bool   use6DOF        = false;  // false = 4-DOF (Align), true = 6-DOF (Refine)
        bool   useCCLibICP    = false;  // true = use CCCoreLib ICP instead of custom Align
    };

    explicit RegistrationWorker(Config cfg, QObject* parent = nullptr);

    // Thread-safe cancel request
    void requestCancel() { cancelRequested_.store(true); }

    // GenericProgressCallback — called from worker thread
    void update(float percent) override;
    void setMethodTitle(const char*) override {}
    void setInfo(const char*) override {}
    void start() override {}
    void stop() override {}
    bool isCancelRequested() override { return cancelRequested_.load(); }

public slots:
    void run();

signals:
    void progressUpdated(float percent);
    void registrationFinished(bool ok,
                              Transformation3D totalTransform,
                              double finalRMS,
                              unsigned finalPointCount,
                              QString errorMsg);

private:
    void run4DOF();      // Align: 4-DOF (alpha + tx, ty, tz)
    void run6DOF();      // Refine: 6-DOF point-to-plane (Neugebauer)
    void runCCLibICP();  // CCCoreLib ICP (full 6-DOF)
    static CCCoreLib::PointCloud buildCloud(const ViffImage& img, const RoiMask* roi);

    Config cfg_;
    std::atomic<bool> cancelRequested_{false};
    float lastEmittedPercent_{-1.0f};
};
