#pragma once

#include "../io/ViffReader.h"
#include "../registration/CoarseRegistration.h"
#include "../registration/RegistrationWorker.h"
#include "../registration/Transformation3D.h"
#include "../RoiMask.h"

#include <QCloseEvent>
#include <QDialog>
#include <QMetaObject>
#include <QThread>
#include <QVector>
#include <functional>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class ImageWindow;

class MatchingControlPanel : public QDialog {
    Q_OBJECT
public:
    // Callback signature: openWindow(image, title, roiMask)
    // roiMask is optional - if provided, it will be copied to the new window
    using OpenWindowCallback = std::function<void(ViffImage, QString, const RoiMask*)>;

    explicit MatchingControlPanel(
        ImageWindow* owner,
        std::function<QVector<ImageWindow*>()> getWindows,
        OpenWindowCallback openWindow,
        QWidget* parent = nullptr);

    // Read back current transformation parameters from spinboxes
    Transformation3D currentTransform() const;

    // Set transformation spinboxes and emit transformChanged
    void setTransform(const Transformation3D& t);

signals:
    void transformChanged(const Transformation3D& t);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

public slots:
    void onClear();
    void onFromPoints();
    void onCancelPicking();
    void onFromCOM();

private slots:
    void onDataLandmarkPicked(QPointF pt);
    void onDataPickingDone();
    void onTargetLandmarkPicked(QPointF pt);
    void onTargetPickingDone();
    // Registration
    void onAlign();      // 4-DOF: alpha + tx, ty, tz
    void onRefine();     // 6-DOF: point-to-plane (Neugebauer)
    void onCCLibICP();   // CCCoreLib ICP (full 6-DOF)
    void onStop();
    void onProgressUpdated(float percent);
    void onRegistrationFinished(bool ok, Transformation3D t,
                                double finalRMS, unsigned finalPointCount,
                                QString errorMsg);
    // Computed images
    void onDiffImage();
    void onCompleted();

public slots:
    void refreshTargetList();

private:
    ImageWindow* selectedTarget() const;
    ImageWindow* selectedData() const;   // the non-target image from the pair
    void startDataPicking();
    void startTargetPicking();
    void finishPicking();
    void cancelPicking();
    void stopWorker();

    ImageWindow* owner_;        // window that opened the panel (header identity only)
    ImageWindow* dataWindow_ = nullptr;  // data image captured when picking starts
    std::function<QVector<ImageWindow*>()> getWindows_;
    OpenWindowCallback openWindow_;

    // Target list
    QListWidget* targetList_ = nullptr;

    // Transformation parameter spinboxes + enable checkboxes
    QCheckBox*      cbAlpha_   = nullptr;  QDoubleSpinBox* sbAlpha_ = nullptr;
    QCheckBox*      cbBeta_    = nullptr;  QDoubleSpinBox* sbBeta_  = nullptr;
    QCheckBox*      cbGamma_   = nullptr;  QDoubleSpinBox* sbGamma_ = nullptr;
    QCheckBox*      cbTx_      = nullptr;  QDoubleSpinBox* sbTx_    = nullptr;
    QCheckBox*      cbTy_      = nullptr;  QDoubleSpinBox* sbTy_    = nullptr;
    QCheckBox*      cbTz_      = nullptr;  QDoubleSpinBox* sbTz_    = nullptr;

    // Picking state buttons/labels
    QPushButton*    btnFromPoints_    = nullptr;
    QPushButton*    btnCancelPick_    = nullptr;
    QLabel*         pickStatusLabel_  = nullptr;

    // Matching parameters
    QSpinBox*       sbIters_    = nullptr;
    QSpinBox*       sbMinPs_    = nullptr;
    QSpinBox*       sbOutlq_    = nullptr;
    QDoubleSpinBox* sbMinMR_    = nullptr;
    QCheckBox*      cbFlip_     = nullptr;
    QCheckBox*      cbMinDiff_  = nullptr;  QDoubleSpinBox* sbMinDiff_ = nullptr;
    QCheckBox*      cbMaxDiff_  = nullptr;  QDoubleSpinBox* sbMaxDiff_ = nullptr;

    // Action buttons (need member pointers to enable/disable during registration)
    QPushButton*    btnAlign_     = nullptr;  // 4-DOF registration
    QPushButton*    btnRefine_    = nullptr;  // 6-DOF point-to-plane
    QPushButton*    btnCCLibICP_  = nullptr;  // CCCoreLib ICP
    QPushButton*    btnStop_      = nullptr;
    QPushButton*    btnDiff_     = nullptr;
    QPushButton*    btnComplete_ = nullptr;

    // ICP progress label
    QLabel*         icpStatusLabel_ = nullptr;

    // ICP worker thread
    QThread*            workerThread_ = nullptr;
    RegistrationWorker* worker_       = nullptr;

    // "From Points" state machine
    enum class PickState { Idle, PickingData, PickingTarget };
    PickState        pickState_ = PickState::Idle;
    QVector<QPointF> dataLandmarks_;
    QVector<QPointF> targetLandmarks_;
    QMetaObject::Connection connDataPicked_;
    QMetaObject::Connection connDataDone_;
    QMetaObject::Connection connTargetPicked_;
    QMetaObject::Connection connTargetDone_;
};
