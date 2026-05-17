#include "MatchingControlPanel.h"
#include "../ImageWindow.h"
#include "../DepthImageView.h"
#include "../ImageProcessor.h"
#include "../registration/DifferenceCalculator.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QPlainTextEdit>
#include <QTextStream>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QSpinBox>
#include <QVBoxLayout>

// ── Constructor ───────────────────────────────────────────────────────────────

MatchingControlPanel::MatchingControlPanel(
    ImageWindow* owner,
    std::function<QVector<ImageWindow*>()> getWindows,
    std::function<void(ViffImage, QString)> openWindow,
    QWidget* parent)
    : QDialog(parent)
    , owner_(owner)
    , getWindows_(std::move(getWindows))
    , openWindow_(std::move(openWindow))
{
    setWindowTitle("Matching Control Panel");
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    // Non-modal: do NOT call setModal(true)

    auto* mainLayout = new QVBoxLayout(this);

    // ── Header label ─────────────────────────────────────────────────────────
    const QString title = QString("*** Matching control panel for '#%1 %2' ***")
        .arg(owner_->imageIndex())
        .arg(QFileInfo(owner_->imagePath()).fileName());
    auto* headerLabel = new QLabel(title);
    headerLabel->setAlignment(Qt::AlignCenter);
    QFont f = headerLabel->font();
    f.setBold(true);
    headerLabel->setFont(f);
    mainLayout->addWidget(headerLabel);

    // ── Target / baseline list ────────────────────────────────────────────────
    auto* targetGroup = new QGroupBox("Target / baseline");
    auto* targetLayout = new QVBoxLayout(targetGroup);
    targetList_ = new QListWidget;
    targetList_->setFixedHeight(100);
    targetLayout->addWidget(targetList_);
    mainLayout->addWidget(targetGroup);

    // ── Transformation parameters ─────────────────────────────────────────────
    auto* transGroup = new QGroupBox("Transformation Parameters");
    auto* transGrid  = new QFormLayout(transGroup);

    auto makeSpinPair = [&](QCheckBox*& cb, QDoubleSpinBox*& sb, const char* label) {
        auto* row = new QWidget;
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(0, 0, 0, 0);
        cb = new QCheckBox;
        sb = new QDoubleSpinBox;
        sb->setRange(-1e9, 1e9);
        sb->setDecimals(4);
        sb->setValue(0.0);
        sb->setMinimumWidth(100);
        hbox->addWidget(cb);
        hbox->addWidget(sb);
        hbox->addStretch();
        transGrid->addRow(label, row);
    };

    // Left column: alpha, beta, gamma; right: tx, ty, tz
    // Use a grid with two form columns
    auto* transWidget = new QWidget;
    auto* transHBox   = new QHBoxLayout(transWidget);
    transHBox->setContentsMargins(0, 0, 0, 0);

    auto* leftForm  = new QFormLayout;
    auto* rightForm = new QFormLayout;
    transHBox->addLayout(leftForm);
    transHBox->addSpacing(16);
    transHBox->addLayout(rightForm);

    auto makeAngle = [&](QCheckBox*& cb, QDoubleSpinBox*& sb, const char* label, QFormLayout* fl) {
        auto* row = new QWidget;
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(0, 0, 0, 0);
        cb = new QCheckBox;
        sb = new QDoubleSpinBox;
        sb->setRange(-360.0, 360.0);
        sb->setDecimals(4);
        sb->setValue(0.0);
        sb->setMinimumWidth(90);
        hbox->addWidget(cb);
        hbox->addWidget(sb);
        fl->addRow(label, row);
    };
    auto makeTrans = [&](QCheckBox*& cb, QDoubleSpinBox*& sb, const char* label, QFormLayout* fl) {
        auto* row = new QWidget;
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(0, 0, 0, 0);
        cb = new QCheckBox;
        sb = new QDoubleSpinBox;
        sb->setRange(-1e9, 1e9);
        sb->setDecimals(4);
        sb->setValue(0.0);
        sb->setMinimumWidth(90);
        hbox->addWidget(cb);
        hbox->addWidget(sb);
        fl->addRow(label, row);
    };

    makeAngle(cbAlpha_, sbAlpha_, "alpha",  leftForm);
    makeAngle(cbBeta_,  sbBeta_,  "beta",   leftForm);
    makeAngle(cbGamma_, sbGamma_, "gamma",  leftForm);
    makeTrans(cbTx_,    sbTx_,    "t_x",    rightForm);
    makeTrans(cbTy_,    sbTy_,    "t_y",    rightForm);
    makeTrans(cbTz_,    sbTz_,    "t_z",    rightForm);

    (void)makeSpinPair; // suppress unused warning — we used inline lambdas

    auto* transGroupLayout = new QVBoxLayout(transGroup);
    transGroupLayout->addWidget(transWidget);
    mainLayout->addWidget(transGroup);

    // ── Grobjustierung buttons ────────────────────────────────────────────────
    auto* coarseRow  = new QHBoxLayout;
    auto* btnClear   = new QPushButton("Clear");
    btnFromPoints_   = new QPushButton("From Points");
    auto* btnFromCOM = new QPushButton("From COM");
    coarseRow->addWidget(btnClear);
    coarseRow->addWidget(btnFromPoints_);
    coarseRow->addWidget(btnFromCOM);
    mainLayout->addLayout(coarseRow);

    // Pick status label + cancel button (hidden when idle)
    auto* pickRow = new QHBoxLayout;
    pickStatusLabel_ = new QLabel;
    pickStatusLabel_->setVisible(false);
    btnCancelPick_ = new QPushButton("Cancel Picking");
    btnCancelPick_->setVisible(false);
    pickRow->addWidget(pickStatusLabel_);
    pickRow->addStretch();
    pickRow->addWidget(btnCancelPick_);
    mainLayout->addLayout(pickRow);

    // ── Matching parameters ───────────────────────────────────────────────────
    auto* matchGroup  = new QGroupBox("Matching Parameters");
    auto* matchLayout = new QVBoxLayout(matchGroup);

    auto* matchRow1 = new QHBoxLayout;
    matchRow1->addWidget(new QLabel("Iter's"));
    sbIters_ = new QSpinBox; sbIters_->setRange(1, 100000); sbIters_->setValue(8000);
    matchRow1->addWidget(sbIters_);
    matchRow1->addWidget(new QLabel("Min.p's"));
    sbMinPs_ = new QSpinBox; sbMinPs_->setRange(1, 100000); sbMinPs_->setValue(600);
    matchRow1->addWidget(sbMinPs_);
    matchRow1->addWidget(new QLabel("Outl.qu."));
    sbOutlq_ = new QSpinBox; sbOutlq_->setRange(0, 100); sbOutlq_->setValue(5);
    matchRow1->addWidget(sbOutlq_);
    matchRow1->addWidget(new QLabel("Min.MR%"));
    sbMinMR_ = new QDoubleSpinBox; sbMinMR_->setRange(0.0, 100.0); sbMinMR_->setValue(4.0);
    matchRow1->addWidget(sbMinMR_);
    cbFlip_ = new QCheckBox("Flip");
    matchRow1->addWidget(cbFlip_);
    matchLayout->addLayout(matchRow1);

    auto* matchRow2 = new QHBoxLayout;
    cbMinDiff_ = new QCheckBox("Min.valid.diff");
    sbMinDiff_ = new QDoubleSpinBox; sbMinDiff_->setRange(-1e9, 1e9); sbMinDiff_->setValue(0.0);
    cbMaxDiff_ = new QCheckBox("Max.valid.diff");
    sbMaxDiff_ = new QDoubleSpinBox; sbMaxDiff_->setRange(-1e9, 1e9); sbMaxDiff_->setValue(0.0);
    matchRow2->addWidget(cbMinDiff_);
    matchRow2->addWidget(sbMinDiff_);
    matchRow2->addSpacing(8);
    matchRow2->addWidget(cbMaxDiff_);
    matchRow2->addWidget(sbMaxDiff_);
    matchLayout->addLayout(matchRow2);
    mainLayout->addWidget(matchGroup);

    // ── Action buttons ────────────────────────────────────────────────────────
    auto* actionRow = new QHBoxLayout;
    btnMatch_    = new QPushButton("Match");
    btnStop_     = new QPushButton("Stop");     btnStop_->setEnabled(false);
    btnDiff_     = new QPushButton("Diff.img."); btnDiff_->setEnabled(false);
    btnComplete_ = new QPushButton("Complete");  btnComplete_->setEnabled(false);
    auto* btnOk      = new QPushButton("Ok");
    auto* btnCancel  = new QPushButton("Cancel");
    actionRow->addWidget(btnMatch_);
    actionRow->addWidget(btnStop_);
    actionRow->addWidget(btnDiff_);
    actionRow->addWidget(btnComplete_);
    actionRow->addStretch();
    actionRow->addWidget(btnOk);
    actionRow->addWidget(btnCancel);
    mainLayout->addLayout(actionRow);

    // ICP status label
    icpStatusLabel_ = new QLabel;
    icpStatusLabel_->setAlignment(Qt::AlignCenter);
    icpStatusLabel_->setVisible(false);
    mainLayout->addWidget(icpStatusLabel_);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(btnClear,       &QPushButton::clicked, this, &MatchingControlPanel::onClear);
    connect(btnFromPoints_, &QPushButton::clicked, this, &MatchingControlPanel::onFromPoints);
    connect(btnFromCOM,     &QPushButton::clicked, this, &MatchingControlPanel::onFromCOM);
    connect(btnCancelPick_, &QPushButton::clicked, this, &MatchingControlPanel::onCancelPicking);
    connect(btnMatch_,    &QPushButton::clicked, this, &MatchingControlPanel::onMatch);
    connect(btnStop_,     &QPushButton::clicked, this, &MatchingControlPanel::onStop);
    connect(btnDiff_,     &QPushButton::clicked, this, &MatchingControlPanel::onDiffImage);
    connect(btnComplete_, &QPushButton::clicked, this, &MatchingControlPanel::onCompleted);
    connect(btnOk,        &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel,    &QPushButton::clicked, this, &QDialog::reject);
    connect(targetList_, &QListWidget::currentRowChanged, this, [this](int) {
        const bool hasTarget = (selectedTarget() != nullptr);
        btnDiff_->setEnabled(hasTarget);
        btnComplete_->setEnabled(hasTarget);
    });

    adjustSize();
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void MatchingControlPanel::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    refreshTargetList();
}

// ── Target list ───────────────────────────────────────────────────────────────

void MatchingControlPanel::refreshTargetList() {
    const int prevIndex = targetList_->currentRow();
    const int prevId = (prevIndex >= 0)
        ? targetList_->item(prevIndex)->data(Qt::UserRole).toInt() : -1;

    targetList_->clear();
    auto* noneItem = new QListWidgetItem("(none)");
    noneItem->setData(Qt::UserRole, -1);
    targetList_->addItem(noneItem);

    for (ImageWindow* w : getWindows_()) {
        if (!w) continue;
        const QString label = QString("#%1 %2")
            .arg(w->imageIndex())
            .arg(QFileInfo(w->imagePath()).fileName());
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, w->imageIndex());
        targetList_->addItem(item);
    }

    // Restore previous selection
    for (int i = 0; i < targetList_->count(); ++i) {
        if (targetList_->item(i)->data(Qt::UserRole).toInt() == prevId) {
            targetList_->setCurrentRow(i);
            break;
        }
    }
    if (targetList_->currentRow() < 0)
        targetList_->setCurrentRow(0);
}

ImageWindow* MatchingControlPanel::selectedTarget() const {
    const auto* item = targetList_->currentItem();
    if (!item) return nullptr;
    const int id = item->data(Qt::UserRole).toInt();
    if (id < 0) return nullptr;
    for (ImageWindow* w : getWindows_()) {
        if (w && w->imageIndex() == id) return w;
    }
    return nullptr;
}

ImageWindow* MatchingControlPanel::selectedData() const {
    ImageWindow* target = selectedTarget();
    for (ImageWindow* w : getWindows_()) {
        if (w && w != target) return w;
    }
    return owner_;  // fallback when pair is not fully resolved
}

// ── Transformation UI helpers ─────────────────────────────────────────────────

void MatchingControlPanel::setTransform(const Transformation3D& t) {
    sbAlpha_->setValue(t.alpha);
    sbBeta_ ->setValue(t.beta);
    sbGamma_->setValue(t.gamma);
    sbTx_   ->setValue(t.tx);
    sbTy_   ->setValue(t.ty);
    sbTz_   ->setValue(t.tz);
    emit transformChanged(t);
}

Transformation3D MatchingControlPanel::currentTransform() const {
    return { sbAlpha_->value(), sbBeta_->value(), sbGamma_->value(),
             sbTx_->value(),    sbTy_->value(),   sbTz_->value() };
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MatchingControlPanel::onClear() {
    cancelPicking();
    setTransform(Transformation3D{});
}

void MatchingControlPanel::onFromCOM() {
    cancelPicking();
    ImageWindow* target = selectedTarget();
    if (!target) {
        QMessageBox::information(this, "From COM", "Please select a target image.");
        return;
    }
    Transformation3D t;
    QString err;
    if (!CoarseRegistration::fromCOM(
            selectedData()->image(), nullptr,
            target->image(), nullptr,
            t, err)) {
        QMessageBox::warning(this, "From COM", err);
        return;
    }
    setTransform(t);
}

void MatchingControlPanel::onFromPoints() {
    if (pickState_ != PickState::Idle) {
        // "Done" clicked — advance state
        if (pickState_ == PickState::PickingData) {
            if (dataLandmarks_.size() < 3) {
                QMessageBox::information(this, "From Points",
                    "Pick at least 3 points in the data image first.");
                return;
            }
            startTargetPicking();
        } else if (pickState_ == PickState::PickingTarget) {
            onTargetPickingDone();
        }
        return;
    }

    ImageWindow* target = selectedTarget();
    if (!target) {
        QMessageBox::information(this, "From Points",
            "Please select a target image first.");
        return;
    }
    dataLandmarks_.clear();
    targetLandmarks_.clear();
    startDataPicking();
}

void MatchingControlPanel::onCancelPicking() {
    cancelPicking();
}

// ── Landmark pick state machine ───────────────────────────────────────────────

void MatchingControlPanel::startDataPicking() {
    dataWindow_ = selectedData();  // capture now; stable for the entire pick sequence
    pickState_ = PickState::PickingData;
    btnFromPoints_->setText("Done picking data →");
    pickStatusLabel_->setText(
        QString("Picking in data image #%1 — click points, Enter or double-click to advance")
        .arg(dataWindow_->imageIndex()));
    pickStatusLabel_->setVisible(true);
    btnCancelPick_->setVisible(true);

    dataWindow_->depthView()->startLandmarkPickMode();
    dataWindow_->raise();

    connDataPicked_ = connect(dataWindow_->depthView(), &DepthImageView::landmarkPicked,
                              this, &MatchingControlPanel::onDataLandmarkPicked);
    connDataDone_   = connect(dataWindow_->depthView(), &DepthImageView::landmarkPickingDone,
                              this, &MatchingControlPanel::onDataPickingDone);
}

void MatchingControlPanel::startTargetPicking() {
    ImageWindow* target = selectedTarget();
    if (!target) { cancelPicking(); return; }

    // Disconnect from data image
    disconnect(connDataPicked_);
    disconnect(connDataDone_);
    dataWindow_->depthView()->stopLandmarkPickMode();

    pickState_ = PickState::PickingTarget;
    btnFromPoints_->setText("Done picking target ✓");
    pickStatusLabel_->setText(
        QString("Picking in target image #%1 — click %2 points, Enter or double-click to compute")
        .arg(target->imageIndex())
        .arg(dataLandmarks_.size()));

    target->depthView()->startLandmarkPickMode();
    target->raise();

    connTargetPicked_ = connect(target->depthView(), &DepthImageView::landmarkPicked,
                                this, &MatchingControlPanel::onTargetLandmarkPicked);
    connTargetDone_   = connect(target->depthView(), &DepthImageView::landmarkPickingDone,
                                this, &MatchingControlPanel::onTargetPickingDone);
}

void MatchingControlPanel::onDataLandmarkPicked(QPointF pt) {
    dataLandmarks_.append(pt);
    pickStatusLabel_->setText(
        QString("Picking in data image #%1 — %2 point(s) picked, Enter/double-click to advance")
        .arg(dataWindow_->imageIndex())
        .arg(dataLandmarks_.size()));

    dataWindow_->depthView()->setLandmarkDisplay(dataLandmarks_);
}

void MatchingControlPanel::onDataPickingDone() {
    if (dataLandmarks_.size() < 3) {
        QMessageBox::information(this, "From Points",
            "Pick at least 3 points in the data image.");
        return;
    }
    startTargetPicking();
}

void MatchingControlPanel::onTargetLandmarkPicked(QPointF pt) {
    targetLandmarks_.append(pt);
    ImageWindow* target = selectedTarget();
    const QString name = target
        ? QString("#%1").arg(target->imageIndex()) : "target";
    pickStatusLabel_->setText(
        QString("Picking in target image %1 — %2/%3 point(s)")
        .arg(name)
        .arg(targetLandmarks_.size())
        .arg(dataLandmarks_.size()));

    if (target)
        target->depthView()->setLandmarkDisplay(targetLandmarks_);

    // Auto-compute when we have enough points
    if (static_cast<int>(targetLandmarks_.size()) == static_cast<int>(dataLandmarks_.size())
            && targetLandmarks_.size() >= 3) {
        onTargetPickingDone();
    }
}

void MatchingControlPanel::onTargetPickingDone() {
    if (targetLandmarks_.size() < 3) {
        QMessageBox::information(this, "From Points",
            "Pick at least 3 corresponding points in the target image.");
        return;
    }
    if (targetLandmarks_.size() != dataLandmarks_.size()) {
        QMessageBox::information(this, "From Points",
            QString("Data has %1 points, target has %2. Counts must match.")
            .arg(dataLandmarks_.size()).arg(targetLandmarks_.size()));
        return;
    }

    ImageWindow* target = selectedTarget();
    if (!target) { cancelPicking(); return; }

    // Build point pair vectors
    std::vector<std::pair<int,int>> dataPts, refPts;
    for (const QPointF& p : dataLandmarks_)
        dataPts.push_back({static_cast<int>(p.x()), static_cast<int>(p.y())});
    for (const QPointF& p : targetLandmarks_)
        refPts.push_back({static_cast<int>(p.x()), static_cast<int>(p.y())});

    Transformation3D t;
    QString err;
    const bool ok = CoarseRegistration::fromPoints(
        dataPts, dataWindow_->image(),
        refPts,  target->image(),
        t, err);

    finishPicking();
    disconnect(connTargetPicked_);
    disconnect(connTargetDone_);
    target->depthView()->stopLandmarkPickMode();
    target->depthView()->clearLandmarkDisplay();

    if (!ok) {
        QMessageBox::warning(this, "From Points", err);
    } else {
        setTransform(t);
    }
}

void MatchingControlPanel::finishPicking() {
    pickState_ = PickState::Idle;
    btnFromPoints_->setText("From Points");
    pickStatusLabel_->setVisible(false);
    btnCancelPick_->setVisible(false);
    if (dataWindow_) dataWindow_->depthView()->clearLandmarkDisplay();
    dataWindow_ = nullptr;
}

void MatchingControlPanel::cancelPicking() {
    if (pickState_ == PickState::Idle) return;

    disconnect(connDataPicked_);
    disconnect(connDataDone_);
    disconnect(connTargetPicked_);
    disconnect(connTargetDone_);

    if (dataWindow_) {
        dataWindow_->depthView()->stopLandmarkPickMode();
        dataWindow_->depthView()->clearLandmarkDisplay();
    }

    ImageWindow* target = selectedTarget();
    if (target) {
        target->depthView()->stopLandmarkPickMode();
        target->depthView()->clearLandmarkDisplay();
    }

    dataLandmarks_.clear();
    targetLandmarks_.clear();
    finishPicking();
}

// ── ICP registration ──────────────────────────────────────────────────────────

void MatchingControlPanel::onMatch() {
    cancelPicking();

    ImageWindow* target = selectedTarget();
    if (!target) {
        QMessageBox::information(this, "Match", "Please select a target/reference image.");
        return;
    }
    stopWorker();  // stop any previous run

    ImageWindow* data = selectedData();
    if (data == target) {
        QMessageBox::information(this, "Match", "Target and data image must be different.");
        return;
    }

    // Build worker config
    RegistrationWorker::Config cfg;
    cfg.modelImg         = target->image();    // reference (will not move)
    cfg.modelRoi         = target->roiMask();
    cfg.dataImg          = data->image();      // data (will be aligned)
    cfg.dataRoi          = data->roiMask();
    cfg.initialTransform = currentTransform();
    cfg.maxIterations    = sbIters_->value();
    cfg.samplingLimit    = std::max(3, sbMinPs_->value());
    const int outlierPct = sbOutlq_->value(); // e.g. 5 = 5% outliers removed
    cfg.overlapRatio     = std::clamp(1.0 - outlierPct / 100.0, 0.01, 1.0);
    cfg.minRMSDecrease   = 1.0e-5;

    // Create worker and thread (worker_ / workerThread_ stay set until finished)
    workerThread_ = new QThread;
    worker_       = new RegistrationWorker(std::move(cfg));
    worker_->moveToThread(workerThread_);

    connect(workerThread_, &QThread::started,
            worker_,       &RegistrationWorker::run);
    connect(worker_, &RegistrationWorker::progressUpdated,
            this,    &MatchingControlPanel::onProgressUpdated);
    connect(worker_, &RegistrationWorker::registrationFinished,
            this,    &MatchingControlPanel::onRegistrationFinished);
    connect(worker_, &RegistrationWorker::registrationFinished,
            workerThread_, &QThread::quit);
    connect(workerThread_, &QThread::finished, worker_,       &QObject::deleteLater);
    connect(workerThread_, &QThread::finished, workerThread_, &QObject::deleteLater);

    btnMatch_->setEnabled(false);
    btnStop_->setEnabled(true);
    icpStatusLabel_->setText("ICP running…");
    icpStatusLabel_->setVisible(true);

    workerThread_->start();
}

void MatchingControlPanel::onStop() {
    if (worker_) worker_->requestCancel();
}

void MatchingControlPanel::stopWorker() {
    if (!workerThread_) return;
    if (worker_) worker_->requestCancel();
    workerThread_->quit();
    workerThread_->wait(5000);
    // Objects scheduled for deletion via deleteLater — just clear our raw pointers.
    // If wait() timed out the objects may still be in use — leak is preferable to crash.
    worker_       = nullptr;
    workerThread_ = nullptr;
}

void MatchingControlPanel::onProgressUpdated(float percent) {
    icpStatusLabel_->setText(QString("ICP: %1%").arg(static_cast<int>(percent)));
}

void MatchingControlPanel::onRegistrationFinished(bool ok,
                                                   Transformation3D t,
                                                   double finalRMS,
                                                   unsigned finalPointCount,
                                                   QString errorMsg) {
    // Worker and thread will be cleaned up via deleteLater (connected to QThread::finished).
    // Clear raw pointers now so stopWorker() / onStop() won't dereference them.
    worker_       = nullptr;
    workerThread_ = nullptr;

    btnMatch_->setEnabled(true);
    btnStop_->setEnabled(false);

    if (ok) {
        setTransform(t);
        icpStatusLabel_->setText(
            QString("Done — RMS: %1  Points: %2")
            .arg(finalRMS, 0, 'f', 4)
            .arg(finalPointCount));
    } else {
        const QString reason = errorMsg.isEmpty() ? "Failed" : errorMsg;
        icpStatusLabel_->setText(QString("ICP stopped: %1").arg(reason));
        if (errorMsg != "Canceled by user" && !errorMsg.isEmpty())
            QMessageBox::warning(this, "ICP Registration", errorMsg);
    }
    icpStatusLabel_->setVisible(true);
}

void MatchingControlPanel::closeEvent(QCloseEvent* event) {
    cancelPicking();
    stopWorker();
    QDialog::closeEvent(event);
}

// ── Difference / Completed image ──────────────────────────────────────────────

void MatchingControlPanel::onDiffImage() {
    ImageWindow* target = selectedTarget();
    if (!target) {
        QMessageBox::information(this, "Diff image", "Please select a target/baseline image.");
        return;
    }

    const Transformation3D t = currentTransform();
    const bool useMin = cbMinDiff_->isChecked();
    const bool useMax = cbMaxDiff_->isChecked();
    const float vMin  = static_cast<float>(sbMinDiff_->value());
    const float vMax  = static_cast<float>(sbMaxDiff_->value());

    ImageWindow* data = selectedData();
    ViffImage diff = DifferenceCalculator::compute(
        target->image(), data->image(), t,
        useMin, vMin, useMax, vMax);

    const auto stats = ImageProcessor::computeStats(diff, nullptr);

    const QString title = QString("diff_%1_minus_%2")
        .arg(QFileInfo(target->imagePath()).baseName())
        .arg(QFileInfo(data->imagePath()).baseName());
    const QString statsText = ImageProcessor::formatStats(stats, title);

    QDialog dlg(this);
    dlg.setWindowTitle("Difference Image Statistics");
    auto* layout = new QVBoxLayout(&dlg);
    auto* edit = new QPlainTextEdit(statsText, &dlg);
    edit->setReadOnly(true);
    edit->setFont(QFont("Courier", 10));
    edit->setMinimumSize(380, 300);
    layout->addWidget(edit);
    auto* buttons = new QDialogButtonBox(&dlg);
    auto* saveBtn = buttons->addButton("Save...", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);
    connect(saveBtn, &QPushButton::clicked, &dlg, [&]{
        const QString path = QFileDialog::getSaveFileName(
            &dlg, "Save statistics", title + "_stats.txt",
            "Text files (*.txt);;All files (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&f) << statsText;
        else
            QMessageBox::warning(&dlg, "Save", "Cannot write file:\n" + path);
    });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    dlg.exec();

    if (openWindow_) openWindow_(std::move(diff), title);
}

void MatchingControlPanel::onCompleted() {
    ImageWindow* target = selectedTarget();
    if (!target) {
        QMessageBox::information(this, "Completed image", "Please select a target/baseline image.");
        return;
    }

    const Transformation3D t = currentTransform();
    ViffImage completed = DifferenceCalculator::computeCompleted(
        target->image(), selectedData()->image(), t);

    const QString title = QString("completed_%1")
        .arg(QFileInfo(target->imagePath()).baseName());

    if (openWindow_) openWindow_(std::move(completed), title);
}
