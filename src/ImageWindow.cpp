#include "ImageWindow.h"
#include "DepthImageView.h"
#include "ImageProcessor.h"
#include "MainWindow.h"
#include "dialogs/HistogramDialog.h"
#include "dialogs/MatchingControlPanel.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QScrollArea>
#include <QToolBar>
#include <QWidget>
#include <cmath>
#include <functional>
#include <limits>

#include "io/PlyIO.h"
#include "io/ViffWriter.h"

#include <QFile>
#include <QFont>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

ImageWindow::ImageWindow(int index, const QString& path,
                         ViffImage image, QWidget* parent)
    : QMainWindow(parent, Qt::Window)
    , index_(index)
    , path_(path)
    , image_(std::move(image))
    , roiMask_(image_.rows, image_.cols, true)
    , origXPixelSize_(image_.xPixelSize)
    , origYPixelSize_(image_.yPixelSize)
{
    const QString title = QString("#%1 %2").arg(index_).arg(QFileInfo(path_).fileName());
    setWindowTitle(title);

    createMenus();
    createToolBar();
    createCentralWidget();
    createStatusBar();

    resize(600, 500);
}

// ── ROI helpers ───────────────────────────────────────────────────────────────

void ImageWindow::applyRoiOp(std::function<void(RoiMask&)> op) {
    op(roiMask_);
    depthView_->roiChanged();
}

void ImageWindow::setRoiMask(const RoiMask& mask) {
    roiMask_ = mask;
    depthView_->roiChanged();
}

// ── Image processing helpers ──────────────────────────────────────────────────

void ImageWindow::applyImgOp(std::function<void(ViffImage&)> op) {
    op(image_);
    depthView_->roiChanged();  // triggers redraw with new data
}

void ImageWindow::showStripDialog(bool horizontal, bool select) {
    QDialog dlg(this);
    dlg.setWindowTitle(horizontal ? (select ? "Select horiz. strip" : "Unselect horiz. strip")
                                  : (select ? "Select vert. strip"  : "Unselect vert. strip"));
    auto* form = new QFormLayout(&dlg);

    const uint32_t maxVal = horizontal ? image_.rows - 1 : image_.cols - 1;
    auto* sb1 = new QSpinBox;  sb1->setRange(0, static_cast<int>(maxVal));
    auto* sb2 = new QSpinBox;  sb2->setRange(0, static_cast<int>(maxVal));
    sb2->setValue(static_cast<int>(maxVal));
    form->addRow(horizontal ? "From row:" : "From col:", sb1);
    form->addRow(horizontal ? "To row:"   : "To col:",   sb2);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    const uint32_t a = static_cast<uint32_t>(sb1->value());
    const uint32_t b = static_cast<uint32_t>(sb2->value());
    if (horizontal) applyRoiOp([=](RoiMask& m){ m.applyHorizStrip(a, b, select); });
    else            applyRoiOp([=](RoiMask& m){ m.applyVertStrip(a, b, select); });
}

void ImageWindow::showEllipseDialog(bool select) {
    QDialog dlg(this);
    dlg.setWindowTitle(select ? "Select ellipse" : "Unselect ellipse");
    auto* form = new QFormLayout(&dlg);
    auto mk = [](double val, double max) {
        auto* sb = new QDoubleSpinBox; sb->setRange(0.0, max); sb->setValue(val); return sb;
    };
    auto* sbCX = mk(image_.cols / 2.0, image_.cols);
    auto* sbCY = mk(image_.rows / 2.0, image_.rows);
    auto* sbRX = mk(image_.cols / 4.0, image_.cols);
    auto* sbRY = mk(image_.rows / 4.0, image_.rows);
    form->addRow("Center col:", sbCX); form->addRow("Center row:", sbCY);
    form->addRow("Radius col:", sbRX); form->addRow("Radius row:", sbRY);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    const float cx = static_cast<float>(sbCX->value()), cy = static_cast<float>(sbCY->value());
    const float rx = static_cast<float>(sbRX->value()), ry = static_cast<float>(sbRY->value());
    applyRoiOp([=](RoiMask& m){ m.applyEllipse(cx, cy, rx, ry, select); });
}

void ImageWindow::showZClipDialog() {
    QDialog dlg(this);
    dlg.setWindowTitle("Clip to Z range");
    auto* form = new QFormLayout(&dlg);
    auto* sbMin = new QDoubleSpinBox; sbMin->setRange(-1e9, 1e9); sbMin->setDecimals(2);
    sbMin->setValue(static_cast<double>(depthView_->clipMin()));
    auto* sbMax = new QDoubleSpinBox; sbMax->setRange(-1e9, 1e9); sbMax->setDecimals(2);
    sbMax->setValue(static_cast<double>(depthView_->clipMax()));
    form->addRow("Z min:", sbMin); form->addRow("Z max:", sbMax);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    const float zMin = static_cast<float>(sbMin->value());
    const float zMax = static_cast<float>(sbMax->value());
    applyRoiOp([this, zMin, zMax](RoiMask& m){ m.clipToZRange(image_, zMin, zMax); });
}

void ImageWindow::showShiftDialog() {
    QDialog dlg(this);
    dlg.setWindowTitle("Shift");
    auto* form = new QFormLayout(&dlg);
    auto* sbDcol = new QSpinBox; sbDcol->setRange(-9999, 9999); sbDcol->setValue(0);
    auto* sbDrow = new QSpinBox; sbDrow->setRange(-9999, 9999); sbDrow->setValue(0);
    auto* sbDz   = new QDoubleSpinBox; sbDz->setRange(-1e9, 1e9); sbDz->setDecimals(2);
    form->addRow("d col (pixels):", sbDcol);
    form->addRow("d row (pixels):", sbDrow);
    form->addRow("d Z (units):",    sbDz);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    const int dc = sbDcol->value(), dr = sbDrow->value();
    const float dz = static_cast<float>(sbDz->value());
    applyImgOp([dc, dr, dz](ViffImage& img){ ImageProcessor::shift(img, dc, dr, dz); });
}

void ImageWindow::showScaleDialog() {
    QDialog dlg(this);
    dlg.setWindowTitle("Scale");
    auto* form = new QFormLayout(&dlg);
    auto mk1 = [](){ auto* s=new QDoubleSpinBox; s->setRange(0.001,1000); s->setValue(1.0); s->setDecimals(4); return s; };
    auto* sbSx = mk1(); auto* sbSy = mk1(); auto* sbSz = mk1();
    form->addRow("Scale X (pixel size):", sbSx);
    form->addRow("Scale Y (pixel size):", sbSy);
    form->addRow("Scale Z (Z values):",   sbSz);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    const float sx = static_cast<float>(sbSx->value());
    const float sy = static_cast<float>(sbSy->value());
    const float sz = static_cast<float>(sbSz->value());
    applyImgOp([this, sx, sy, sz](ViffImage& img){
        ImageProcessor::scaleZ(img, &roiMask_, sx, sy, sz);
    });
}

void ImageWindow::showStatisticsDialog() {
    const auto s   = ImageProcessor::computeStats(image_, &roiMask_);
    const QString label = QFileInfo(path_).fileName();
    const QString text  = ImageProcessor::formatStats(s, label);

    QDialog dlg(this);
    dlg.setWindowTitle("Statistics — " + label);
    auto* layout = new QVBoxLayout(&dlg);

    auto* edit = new QPlainTextEdit(text, &dlg);
    edit->setReadOnly(true);
    edit->setFont(QFont("Courier", 10));
    edit->setMinimumSize(380, 300);
    layout->addWidget(edit);

    auto* buttons = new QDialogButtonBox(&dlg);
    auto* saveBtn = buttons->addButton("Save...", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(saveBtn, &QPushButton::clicked, &dlg, [&]{
        QString path = QFileDialog::getSaveFileName(
            &dlg, "Save statistics", label + "_stats.txt",
            "Text files (*.txt);;All files (*)");
        if (path.isEmpty()) return;
        // Auto-append .txt if no extension present
        if (!path.contains('.'))
            path += ".txt";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&f) << text;
        else
            QMessageBox::warning(&dlg, "Save", "Cannot write file:\n" + path);
    });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);

    dlg.exec();
}

void ImageWindow::showHistogramDialog() {
    HistogramDialog dlg(image_, &roiMask_, depthView_,
        [this](float zMin, float zMax){
            applyRoiOp([this, zMin, zMax](RoiMask& m){ m.clipToZRange(image_, zMin, zMax); });
        }, this);
    dlg.exec();
}

void ImageWindow::showFitPlaneDialog() {
    const auto fit = ImageProcessor::fitPlane(image_, &roiMask_);
    const QString label = QFileInfo(path_).fileName();
    const QString text = ImageProcessor::formatPlaneFit(fit, label);

    QDialog dlg(this);
    dlg.setWindowTitle("Fit Plane — " + label);
    auto* layout = new QVBoxLayout(&dlg);

    auto* edit = new QPlainTextEdit(text, &dlg);
    edit->setReadOnly(true);
    edit->setFont(QFont("Courier", 10));
    edit->setMinimumSize(400, 280);
    layout->addWidget(edit);

    auto* buttons = new QDialogButtonBox(&dlg);
    auto* subtractBtn = buttons->addButton("Subtract Plane", QDialogButtonBox::ActionRole);
    auto* saveBtn = buttons->addButton("Save...", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    subtractBtn->setEnabled(fit.valid);

    connect(subtractBtn, &QPushButton::clicked, &dlg, [this, &dlg, fit, label]{
        ViffImage result = ImageProcessor::subtractPlane(image_, fit);
        const QString title = label + " - plane subtracted";
        if (mainWindow_) {
            mainWindow_->openImageWindow(std::move(result), title, nullptr);
        }
        dlg.accept();
    });

    connect(saveBtn, &QPushButton::clicked, &dlg, [&dlg, &text, label]{
        QString path = QFileDialog::getSaveFileName(
            &dlg, "Save plane fit", label + "_planefit.txt",
            "Text files (*.txt);;All files (*)");
        if (path.isEmpty()) return;
        if (!path.contains('.'))
            path += ".txt";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&f) << text;
        else
            QMessageBox::warning(&dlg, "Save", "Cannot write file:\n" + path);
    });

    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);

    dlg.exec();
}

void ImageWindow::showFitSphereDialog() {
    const auto fit = ImageProcessor::fitSphere(image_, &roiMask_);
    const QString label = QFileInfo(path_).fileName();
    const QString text = ImageProcessor::formatSphereFit(fit, label);

    QDialog dlg(this);
    dlg.setWindowTitle("Fit Sphere — " + label);
    auto* layout = new QVBoxLayout(&dlg);

    auto* edit = new QPlainTextEdit(text, &dlg);
    edit->setReadOnly(true);
    edit->setFont(QFont("Courier", 10));
    edit->setMinimumSize(400, 320);
    layout->addWidget(edit);

    auto* buttons = new QDialogButtonBox(&dlg);
    auto* subtractBtn = buttons->addButton("Subtract Sphere", QDialogButtonBox::ActionRole);
    auto* saveBtn = buttons->addButton("Save...", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    subtractBtn->setEnabled(fit.valid);

    connect(subtractBtn, &QPushButton::clicked, &dlg, [this, &dlg, fit, label]{
        ViffImage result = ImageProcessor::subtractSphere(image_, fit);
        const QString title = label + " - sphere subtracted";
        if (mainWindow_) {
            mainWindow_->openImageWindow(std::move(result), title, nullptr);
        }
        dlg.accept();
    });

    connect(saveBtn, &QPushButton::clicked, &dlg, [&dlg, &text, label]{
        QString path = QFileDialog::getSaveFileName(
            &dlg, "Save sphere fit", label + "_spherefit.txt",
            "Text files (*.txt);;All files (*)");
        if (path.isEmpty()) return;
        if (!path.contains('.'))
            path += ".txt";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&f) << text;
        else
            QMessageBox::warning(&dlg, "Save", "Cannot write file:\n" + path);
    });

    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);

    dlg.exec();
}

// ── Slot: polygon completed ───────────────────────────────────────────────────

void ImageWindow::onPolygonCompleted(QPolygonF poly, bool select) {
    applyRoiOp([&poly, select](RoiMask& m){ m.applyPolygon(poly, select); });
    statusBar()->clearMessage();
}

// ── Match panel ───────────────────────────────────────────────────────────────

void ImageWindow::showMatchingControlPanel() {
    if (!matchingPanel_) {
        matchingPanel_ = new MatchingControlPanel(
            this,
            [this]() -> QVector<ImageWindow*> {
                return mainWindow_ ? mainWindow_->selectedPair() : QVector<ImageWindow*>{};
            },
            [this](ViffImage img, QString title, const RoiMask* roiMask) {
                if (mainWindow_) mainWindow_->openImageWindow(std::move(img), title, roiMask);
            },
            this);
        matchingPanel_->setAttribute(Qt::WA_DeleteOnClose);
        connect(matchingPanel_, &QObject::destroyed, this,
                [this]{ matchingPanel_ = nullptr; });
        connect(matchingPanel_, &MatchingControlPanel::transformChanged, this,
                [this](const Transformation3D& t){ matchTransform_ = t; });
        if (mainWindow_)
            connect(mainWindow_, &MainWindow::imageWindowClosed,
                    matchingPanel_, &MatchingControlPanel::refreshTargetList);
        matchingPanel_->setTransform(matchTransform_);
    }
    matchingPanel_->show();
    matchingPanel_->raise();
    matchingPanel_->activateWindow();
}

// ── Menu creation ─────────────────────────────────────────────────────────────

void ImageWindow::createMenus() {
    // ── File ──────────────────────────────────────────────────────────────────
    auto* fileMenu = menuBar()->addMenu("&File");
    auto* actSaveViff = fileMenu->addAction("Save &VIFF...");
    connect(actSaveViff, &QAction::triggered, this, [this] {
        const QString p = QFileDialog::getSaveFileName(
            this, "Save VIFF", path_, "VIFF/XV Files (*.xv *.viff);;All Files (*)");
        if (p.isEmpty()) return;
        ViffWriter w;
        if (!w.save(p.toStdString(), image_))
            QMessageBox::warning(this, "Save error", QString::fromStdString(w.lastError()));
    });
    auto* actSavePly = fileMenu->addAction("Save &PLY...");
    connect(actSavePly, &QAction::triggered, this, [this] {
        const QString p = QFileDialog::getSaveFileName(
            this, "Save PLY", path_, "PLY Files (*.ply);;All Files (*)");
        if (p.isEmpty()) return;
        std::string err;
        if (!PlyIO::write(p.toStdString(), image_, err))
            QMessageBox::warning(this, "Save PLY error", QString::fromStdString(err));
    });
    fileMenu->addAction("Save &ASCII...")->setEnabled(false);
    fileMenu->addAction("Save ROI only...")->setEnabled(false);
    fileMenu->addAction("Export &TIFF...")->setEnabled(false);
    fileMenu->addSeparator();
    connect(fileMenu->addAction("&Close"), &QAction::triggered, this, &QMainWindow::close);

    // ── Edit ──────────────────────────────────────────────────────────────────
    auto* editMenu = menuBar()->addMenu("&Edit");

    connect(editMenu->addAction("Select all"), &QAction::triggered, this,
            [this]{ applyRoiOp([](RoiMask& m){ m.selectAll(); }); });
    connect(editMenu->addAction("Unselect all"), &QAction::triggered, this,
            [this]{ applyRoiOp([](RoiMask& m){ m.unselectAll(); }); });
    editMenu->addSeparator();
    connect(editMenu->addAction("Select polygon"), &QAction::triggered, this, [this]{
        depthView_->startPolygonMode(true);
        statusBar()->showMessage(
            "Select polygon: left-click to add vertices — right-click to close — Backspace to undo last point — Esc to cancel");
    });
    connect(editMenu->addAction("Unselect polygon"), &QAction::triggered, this, [this]{
        depthView_->startPolygonMode(false);
        statusBar()->showMessage(
            "Unselect polygon: left-click to add vertices — right-click to close — Backspace to undo last point — Esc to cancel");
    });
    connect(editMenu->addAction("Select complement"), &QAction::triggered, this,
            [this]{ applyRoiOp([](RoiMask& m){ m.invert(); }); });
    editMenu->addSeparator();
    connect(editMenu->addAction("Select horiz. strip"),   &QAction::triggered, this,
            [this]{ showStripDialog(true,  true); });
    connect(editMenu->addAction("Unselect horiz. strip"), &QAction::triggered, this,
            [this]{ showStripDialog(true,  false); });
    connect(editMenu->addAction("Select vert. strip"),    &QAction::triggered, this,
            [this]{ showStripDialog(false, true); });
    connect(editMenu->addAction("Unselect vert. strip"),  &QAction::triggered, this,
            [this]{ showStripDialog(false, false); });
    connect(editMenu->addAction("Select ellipse"),        &QAction::triggered, this,
            [this]{ showEllipseDialog(true); });
    connect(editMenu->addAction("Unselect ellipse"),      &QAction::triggered, this,
            [this]{ showEllipseDialog(false); });
    editMenu->addSeparator();
    connect(editMenu->addAction("Clip to Z range"), &QAction::triggered, this,
            [this]{ showZClipDialog(); });
    connect(editMenu->addAction("Clip to gradient..."), &QAction::triggered, this, [this]{
        QDialog dlg(this);
        dlg.setWindowTitle("Clip to gradient");
        auto* form = new QFormLayout(&dlg);
        auto* sbAngle = new QDoubleSpinBox;
        sbAngle->setRange(1.0, 89.0);
        sbAngle->setDecimals(1);
        sbAngle->setSingleStep(5.0);
        sbAngle->setSuffix(" °");
        sbAngle->setValue(45.0);
        form->addRow("Max slope angle (from XY plane):", sbAngle);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() != QDialog::Accepted) return;
        const float angle = static_cast<float>(sbAngle->value());
        applyRoiOp([this, angle](RoiMask& m){ m.clipToGradient(image_, angle); });
    });
    connect(editMenu->addAction("Commit selection"), &QAction::triggered, this, [this]{
        roiMask_.commitToImage(image_);
        roiMask_.selectAll();
        depthView_->roiChanged();
    });
    editMenu->addSeparator();
    connect(editMenu->addAction("Load polygon..."), &QAction::triggered, this, [this]{
        const QString p = QFileDialog::getOpenFileName(
            this, "Load polygon", QString(), "Text files (*.txt);;All files (*)");
        if (p.isEmpty()) return;
        QPolygonF poly;
        if (!RoiMask::loadPolygon(p, poly)) {
            QMessageBox::warning(this, "Load polygon", "Failed to load polygon file."); return;
        }
        applyRoiOp([&poly](RoiMask& m){ m.applyPolygon(poly, true); });
    });

    // ── Z Range ───────────────────────────────────────────────────────────────
    auto* zMenu = menuBar()->addMenu("&Z Range");

    connect(zMenu->addAction("Set global Z range..."), &QAction::triggered, this, [this]{
        QDialog dlg(this); dlg.setWindowTitle("Set global Z range");
        auto* form = new QFormLayout(&dlg);
        auto* sbMin = new QDoubleSpinBox; sbMin->setRange(-1e9,1e9); sbMin->setDecimals(2);
        sbMin->setValue(static_cast<double>(depthView_->clipMin()));
        auto* sbMax = new QDoubleSpinBox; sbMax->setRange(-1e9,1e9); sbMax->setDecimals(2);
        sbMax->setValue(static_cast<double>(depthView_->clipMax()));
        form->addRow("Z min:", sbMin); form->addRow("Z max:", sbMax);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() != QDialog::Accepted) return;
        depthView_->setClipRange(static_cast<float>(sbMin->value()),
                                  static_cast<float>(sbMax->value()));
    });

    connect(zMenu->addAction("Subtract global minimum"), &QAction::triggered, this, [this]{
        applyImgOp([this](ViffImage& img){
            ImageProcessor::subtractGlobalMin(img, &roiMask_);
        });
        depthView_->setClipRange(0.0f, depthView_->clipMax() - depthView_->clipMin());
    });

    connect(zMenu->addAction("Subtract point 0"), &QAction::triggered, this, [this]{
        applyImgOp([](ViffImage& img){ ImageProcessor::subtractPoint0(img); });
    });

    connect(zMenu->addAction("Reset Z range"), &QAction::triggered, this, [this]{
        float zMin = std::numeric_limits<float>::max();
        float zMax = std::numeric_limits<float>::lowest();
        for (uint32_t r = 0; r < image_.rows; ++r)
            for (uint32_t c = 0; c < image_.cols; ++c)
                if (image_.isValid(r, c)) {
                    zMin = std::min(zMin, image_.at(r, c));
                    zMax = std::max(zMax, image_.at(r, c));
                }
        if (zMin < zMax) depthView_->setClipRange(zMin, zMax);
    });

    connect(zMenu->addAction("Adjust Z range for ROI"), &QAction::triggered, this, [this]{
        float zMin = std::numeric_limits<float>::max();
        float zMax = std::numeric_limits<float>::lowest();
        for (uint32_t r = 0; r < image_.rows; ++r)
            for (uint32_t c = 0; c < image_.cols; ++c)
                if (image_.isValid(r, c) && roiMask_.isSelected(r, c)) {
                    zMin = std::min(zMin, image_.at(r, c));
                    zMax = std::max(zMax, image_.at(r, c));
                }
        if (zMin < zMax) depthView_->setClipRange(zMin, zMax);
    });

    zMenu->addSeparator();
    connect(zMenu->addAction("Histogram..."), &QAction::triggered, this,
            [this]{ showHistogramDialog(); });

    // ── Process ───────────────────────────────────────────────────────────────
    auto* procMenu = menuBar()->addMenu("&Process");
    procMenu->addAction("Parameters...")->setEnabled(false);

    // Filter submenu
    auto* filterMenu = procMenu->addMenu("Filter");
    auto addFilter = [&](const char* label, int ksize,
                          std::function<void(ViffImage&, int)> fn) {
        connect(filterMenu->addAction(label), &QAction::triggered, this,
                [this, ksize, fn]{ applyImgOp([ksize, fn](ViffImage& img){ fn(img, ksize); }); });
    };
    addFilter("Median 3×3",  3, ImageProcessor::medianFilter);
    addFilter("Median 5×5",  5, ImageProcessor::medianFilter);
    addFilter("Median 7×7",  7, ImageProcessor::medianFilter);
    addFilter("Median 9×9",  9, ImageProcessor::medianFilter);
    addFilter("Median 11×11",11, ImageProcessor::medianFilter);
    filterMenu->addSeparator();
    addFilter("Complete 3×3",  3, ImageProcessor::completeFilter);
    addFilter("Complete 5×5",  5, ImageProcessor::completeFilter);
    addFilter("Complete 7×7",  7, ImageProcessor::completeFilter);
    addFilter("Complete 9×9",  9, ImageProcessor::completeFilter);
    addFilter("Complete 11×11",11, ImageProcessor::completeFilter);
    filterMenu->addSeparator();
    for (int k : {3, 5, 7}) {
        connect(filterMenu->addAction(QString("Clip outliers %1×%1").arg(k)),
                &QAction::triggered, this, [this, k]{
                    constexpr float kDev = 200.0f;
                    applyImgOp([k](ViffImage& img){ ImageProcessor::clipOutliers(img, k, kDev); });
                });
    }
    filterMenu->addSeparator();
    connect(filterMenu->addAction("Thin out 3×3"), &QAction::triggered, this,
            [this]{ applyImgOp([](ViffImage& img){ ImageProcessor::thinOut3x3(img); }); });
    connect(filterMenu->addAction("Add noise..."), &QAction::triggered, this, [this]{
        QDialog dlg(this); dlg.setWindowTitle("Add Gaussian noise");
        auto* form = new QFormLayout(&dlg);
        auto* sbSigma = new QDoubleSpinBox; sbSigma->setRange(0.1, 1e6); sbSigma->setValue(50.0);
        form->addRow("Sigma:", sbSigma);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        if (dlg.exec() != QDialog::Accepted) return;
        const float sigma = static_cast<float>(sbSigma->value());
        applyImgOp([this, sigma](ViffImage& img){
            ImageProcessor::addGaussianNoise(img, &roiMask_, sigma);
        });
    });

    // Transform submenu
    auto* transformMenu = procMenu->addMenu("Transform");
    connect(transformMenu->addAction("Mirror X"), &QAction::triggered, this,
            [this]{ applyImgOp([](ViffImage& img){ ImageProcessor::mirrorX(img); }); });
    connect(transformMenu->addAction("Shift..."),  &QAction::triggered, this,
            [this]{ showShiftDialog(); });
    connect(transformMenu->addAction("Scale..."),  &QAction::triggered, this,
            [this]{ showScaleDialog(); });
    connect(transformMenu->addAction("Scale to original"), &QAction::triggered, this, [this]{
        applyImgOp([this](ViffImage& img){
            if (origXPixelSize_ > 0 && origYPixelSize_ > 0) {
                img.xPixelSize = origXPixelSize_;
                img.yPixelSize = origYPixelSize_;
            }
        });
    });

    // Fit Surface submenu
    auto* fitMenu = procMenu->addMenu("Fit Surface");
    connect(fitMenu->addAction("Fit Plane..."), &QAction::triggered, this,
            [this]{ showFitPlaneDialog(); });
    connect(fitMenu->addAction("Fit Sphere..."), &QAction::triggered, this,
            [this]{ showFitSphereDialog(); });

    procMenu->addSeparator();
    connect(procMenu->addAction("Statistics..."), &QAction::triggered, this,
            [this]{ showStatisticsDialog(); });
    procMenu->addAction("Zoom...")->setEnabled(false);
    procMenu->addAction("Show Slice...")->setEnabled(false);
    procMenu->addAction("Show 3D View...")->setEnabled(false);

    // ── Match ─────────────────────────────────────────────────────────────────
    auto* matchMenu = menuBar()->addMenu("&Match");
    connect(matchMenu->addAction("Parameters..."), &QAction::triggered,
            this, &ImageWindow::showMatchingControlPanel);

    connect(matchMenu->addAction("Save parameters..."), &QAction::triggered, this, [this]{
        const QString p = QFileDialog::getSaveFileName(
            this, "Save parameters", path_, "Parameter files (*.par);;All files (*)");
        if (p.isEmpty()) return;
        const Transformation3D t = matchingPanel_ ? matchingPanel_->currentTransform() : matchTransform_;
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Save parameters", "Cannot open file for writing."); return;
        }
        QTextStream out(&f);
        out.setRealNumberPrecision(12);
        out << "alpha " << t.alpha << "\n"
            << "beta "  << t.beta  << "\n"
            << "gamma " << t.gamma << "\n"
            << "tx "    << t.tx    << "\n"
            << "ty "    << t.ty    << "\n"
            << "tz "    << t.tz    << "\n";
    });

    connect(matchMenu->addAction("Load parameters..."), &QAction::triggered, this, [this]{
        const QString p = QFileDialog::getOpenFileName(
            this, "Load parameters", path_, "Parameter files (*.par);;All files (*)");
        if (p.isEmpty()) return;
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Load parameters", "Cannot open file."); return;
        }
        QTextStream in(&f);
        Transformation3D t;
        QString key;
        while (!in.atEnd()) {
            in >> key;
            if      (key == "alpha") in >> t.alpha;
            else if (key == "beta")  in >> t.beta;
            else if (key == "gamma") in >> t.gamma;
            else if (key == "tx")    in >> t.tx;
            else if (key == "ty")    in >> t.ty;
            else if (key == "tz")    in >> t.tz;
            in.readLine();
        }
        matchTransform_ = t;
        if (matchingPanel_) matchingPanel_->setTransform(t);
    });

    matchMenu->addSeparator();

    connect(matchMenu->addAction("Save transformation..."), &QAction::triggered, this, [this]{
        const QString p = QFileDialog::getSaveFileName(
            this, "Save transformation", path_, "Transform files (*.tfm);;All files (*)");
        if (p.isEmpty()) return;
        const Transformation3D t = matchingPanel_ ? matchingPanel_->currentTransform() : matchTransform_;
        const auto cc = t.toCCTransform();
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Save transformation", "Cannot open file for writing."); return;
        }
        QTextStream out(&f);
        out.setRealNumberPrecision(12);
        for (int i = 0; i < 3; ++i) {
            out << cc.R.getValue(i, 0) << " " << cc.R.getValue(i, 1) << " "
                << cc.R.getValue(i, 2) << " "
                << (i == 0 ? cc.T.x : i == 1 ? cc.T.y : cc.T.z) << "\n";
        }
        out << "0 0 0 1\n";
    });

    connect(matchMenu->addAction("Load transformation..."), &QAction::triggered, this, [this]{
        const QString p = QFileDialog::getOpenFileName(
            this, "Load transformation", path_, "Transform files (*.tfm);;All files (*)");
        if (p.isEmpty()) return;
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Load transformation", "Cannot open file."); return;
        }
        QTextStream in(&f);
        double m[4][4] = {};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                in >> m[i][j];
        CCCoreLib::PointProjectionTools::Transformation cc;
        cc.R = CCCoreLib::SquareMatrixd(3);
        cc.s = 1.0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                cc.R.setValue(i, j, m[i][j]);
        cc.T = CCVector3d(m[0][3], m[1][3], m[2][3]);
        const Transformation3D t = Transformation3D::fromCCTransform(cc);
        matchTransform_ = t;
        if (matchingPanel_) matchingPanel_->setTransform(t);
    });
    matchMenu->addSeparator();
    matchMenu->addAction("Start matching...")->setEnabled(false);
    matchMenu->addAction("Difference image")->setEnabled(false);
    matchMenu->addAction("Completed image")->setEnabled(false);
    matchMenu->addAction("Rotated image")->setEnabled(false);
    matchMenu->addSeparator();
    connect(matchMenu->addAction("From points"), &QAction::triggered, this, [this]{
        showMatchingControlPanel();
        matchingPanel_->onFromPoints();
    });
    connect(matchMenu->addAction("From COM..."), &QAction::triggered, this, [this]{
        showMatchingControlPanel();
        matchingPanel_->onFromCOM();
    });
    connect(matchMenu->addAction("Clear parameters"), &QAction::triggered, this, [this]{
        showMatchingControlPanel();
        matchingPanel_->onClear();
    });
}

void ImageWindow::createToolBar() {
    auto* tb = addToolBar("Controls");
    tb->setMovable(false);
    tb->setFloatable(false);

    tb->addWidget(new QLabel("Style  "));
    styleCombo_ = new QComboBox;
    styleCombo_->addItems({"Linear", "False color", "Medium gray", "Linear 2", "Graycast"});
    tb->addWidget(styleCombo_);

    tb->addSeparator();
    tb->addWidget(new QLabel("  Sel. "));
    auto* group = new QButtonGroup(this);
    radioAll_ = new QRadioButton("All"); radioAll_->setChecked(true);
    radioRoi_ = new QRadioButton("ROI");
    group->addButton(radioAll_); group->addButton(radioRoi_);
    tb->addWidget(radioAll_); tb->addWidget(radioRoi_);

    tb->addSeparator();
    crosshCheck_ = new QCheckBox("Crossh.");
    tb->addWidget(crosshCheck_);
}

void ImageWindow::createCentralWidget() {
    depthView_ = new DepthImageView(image_, this);
    depthView_->setRoiMask(&roiMask_);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(depthView_);
    scrollArea->setWidgetResizable(false);
    scrollArea->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setCentralWidget(scrollArea);

    connect(styleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){
                depthView_->setStyle(static_cast<ImageWindow::Style>(idx));
            });
    connect(radioRoi_, &QRadioButton::toggled, this, [this](bool roiOnly){
        depthView_->setRoiOnly(roiOnly);
    });
    connect(depthView_, &DepthImageView::pixelHovered, this,
            [this](int col, int row, float z) {
                // Convert pixel coordinates to world coordinates (mm)
                const float x_mm = col * image_.xPixelSize;
                const float y_mm = row * image_.yPixelSize;
                if (std::isnan(z))
                    coordLabel_->setText(QString("x=%1 mm  y=%2 mm  z=---")
                        .arg(x_mm, 0, 'f', 3).arg(y_mm, 0, 'f', 3));
                else
                    coordLabel_->setText(QString("x=%1 mm  y=%2 mm  z=%3 mm")
                        .arg(x_mm, 0, 'f', 3).arg(y_mm, 0, 'f', 3).arg(z, 0, 'f', 4));
            });
    connect(depthView_, &DepthImageView::pixelLeft, this,
            [this]{ coordLabel_->setText("x=         y=         z=         "); });
    connect(depthView_, &DepthImageView::polygonCompleted,
            this, &ImageWindow::onPolygonCompleted);
}

void ImageWindow::createStatusBar() {
    coordLabel_ = new QLabel("x=         y=         z=         ");
    statusBar()->addWidget(coordLabel_);
}

void ImageWindow::closeEvent(QCloseEvent* event) {
    emit windowClosing(index_);
    event->accept();
}
