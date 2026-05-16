#pragma once

#include "../io/ViffReader.h"
#include "../registration/CoarseRegistration.h"
#include "../registration/RegistrationWorker.h"
#include "../registration/Transformation3D.h"

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
    explicit MatchingControlPanel(
        ImageWindow* owner,
        std::function<QVector<ImageWindow*>()> getWindows,
        std::function<void(ViffImage, QString)> openWindow,
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
    // ICP
    void onMatch();
    void onStop();
    void onProgressUpdated(float percent);
    void onRegistrationFinished(bool ok, Transformation3D t,
                                double finalRMS, unsigned finalPointCount,
                                QString errorMsg);
    // Computed images
    void onDiffImage();
    void onCompleted();

private:
    void refreshTargetList();
    ImageWindow* selectedTarget() const;
    void startDataPicking();
    void startTargetPicking();
    void finishPicking();
    void cancelPicking();
    void stopWorker();

    ImageWindow* owner_;
    std::function<QVector<ImageWindow*>()> getWindows_;
    std::function<void(ViffImage, QString)> openWindow_;

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

    // Action buttons (need member pointers to enable/disable during ICP)
    QPushButton*    btnMatch_    = nullptr;
    QPushButton*    btnStop_     = nullptr;
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
