/*
 * Match3D+ - Dental surface comparison software
 * Copyright (C) 2026 Karl-Heinz Kunzelmann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "MainWindow.h"
#include "ImageWindow.h"
#include "dialogs/GlobalParametersDialog.h"
#include "io/PlyIO.h"
#include "io/ViffReader.h"

#include <QAction>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

static constexpr int kIndexRole = Qt::UserRole;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Match3D 2.5");
    createActions();
    createMenus();
    createCentralWidget();

    QSettings s;
    settings_.load(s);
    lastDir_ = s.value("MainWindow/lastDir", QString()).toString();
    const QByteArray geo = s.value("MainWindow/geometry").toByteArray();
    if (!geo.isEmpty())
        restoreGeometry(geo);
    else
        resize(340, 260);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Close all image windows first
    // Copy list since closing triggers onImageWindowClosing which modifies imageWindows_
    const QVector<ImageWindow*> wins = imageWindows_;
    for (ImageWindow* w : wins) {
        if (w)
            w->close();
    }

    QSettings s;
    settings_.save(s);
    s.setValue("MainWindow/lastDir", lastDir_);
    s.setValue("MainWindow/geometry", saveGeometry());
    QMainWindow::closeEvent(event);
}

void MainWindow::createActions() {
    actOpenViff_ = new QAction("Open &VIFF", this);
    connect(actOpenViff_, &QAction::triggered, this, &MainWindow::onOpenViff);

    actOpenPly_ = new QAction("Open &PLY", this);
    connect(actOpenPly_, &QAction::triggered, this, &MainWindow::onOpenPly);

    actCloseAll_ = new QAction("Close all", this);
    connect(actCloseAll_, &QAction::triggered, this, &MainWindow::onCloseAll);

    actQuit_ = new QAction("&Quit", this);
    actQuit_->setShortcut(QKeySequence::Quit);
    connect(actQuit_, &QAction::triggered, this, &QMainWindow::close);

    actGlobalParams_ = new QAction("Global parameters...", this);
    connect(actGlobalParams_, &QAction::triggered, this, &MainWindow::onGlobalParameters);

    actSetClipFromImg1_ = new QAction("Set clip range from img.1", this);
    actSetClipFromImg1_->setEnabled(false);

    actTransferPolygon_ = new QAction("Transfer polygon", this);
    actTransferPolygon_->setEnabled(false);

    actExtend1to2_ = new QAction("Extend 1 to 2", this);
    actExtend1to2_->setEnabled(false);

    actMerge1in2_ = new QAction("Merge 1 in 2", this);
    actMerge1in2_->setEnabled(false);

    actDiff2D_ = new QAction("2D-Difference 1-2", this);
    actDiff2D_->setEnabled(false);
}

void MainWindow::createMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(actOpenViff_);
    fileMenu->addAction(actOpenPly_);
    fileMenu->addSeparator();
    fileMenu->addAction(actCloseAll_);
    fileMenu->addSeparator();
    fileMenu->addAction(actQuit_);

    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(actGlobalParams_);
    editMenu->addAction(actSetClipFromImg1_);
    editMenu->addAction(actTransferPolygon_);

    auto* modelMenu = menuBar()->addMenu("&Model");
    modelMenu->addAction(actExtend1to2_);
    modelMenu->addAction(actMerge1in2_);
    modelMenu->addAction(actDiff2D_);

    auto* helpMenu = menuBar()->addMenu("&Help");
    auto* actAbout = helpMenu->addAction("About");
    connect(actAbout, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::createCentralWidget() {
    auto* central = new QWidget(this);

    imageList1_ = new QListWidget;
    imageList1_->setMinimumHeight(80);
    imageList1_->setStyleSheet("background: black; color: white;");
    imageList1_->addItem("(none)");

    imageList2_ = new QListWidget;
    imageList2_->setMinimumHeight(80);
    imageList2_->setStyleSheet("background: black; color: white;");
    imageList2_->addItem("(none)");

    auto* row1 = new QHBoxLayout;
    row1->addWidget(new QLabel("Image 1"), 0);
    row1->addWidget(imageList1_, 1);

    auto* row2 = new QHBoxLayout;
    row2->addWidget(new QLabel("Image 2"), 0);
    row2->addWidget(imageList2_, 1);

    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addLayout(row1);
    layout->addLayout(row2);

    setCentralWidget(central);

    connect(imageList1_, &QListWidget::currentItemChanged,
            this, &MainWindow::onImage1SelectionChanged);
    connect(imageList2_, &QListWidget::currentItemChanged,
            this, &MainWindow::onImage2SelectionChanged);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void MainWindow::addImageToLists(int index, const QString& name) {
    const QString entry = QString("#%1 %2").arg(index).arg(name);

    // Remove the "(none)" placeholder from both lists on first image
    if (imageList1_->count() == 1 && imageList1_->item(0)->text() == "(none)")
        imageList1_->clear();
    if (imageList2_->count() == 1 && imageList2_->item(0)->text() == "(none)")
        imageList2_->clear();

    auto* item1 = new QListWidgetItem(entry, imageList1_);
    item1->setData(kIndexRole, index);
    auto* item2 = new QListWidgetItem(entry, imageList2_);
    item2->setData(kIndexRole, index);
}

void MainWindow::removeImageFromLists(int index) {
    for (QListWidget* list : {imageList1_, imageList2_}) {
        for (int r = 0; r < list->count(); ++r) {
            if (list->item(r)->data(kIndexRole).toInt() == index) {
                delete list->takeItem(r);
                break;
            }
        }
        if (list->count() == 0) {
            auto* none = new QListWidgetItem("(none)", list);
            none->setData(kIndexRole, -1);
        }
    }
}

void MainWindow::raiseImageWindow(int index) {
    if (index < 0 || index >= imageWindows_.size()) return;
    ImageWindow* win = imageWindows_[index];
    if (!win) return;
    win->show();
    win->raise();
    win->activateWindow();
}

QVector<ImageWindow*> MainWindow::imageWindows() const {
    QVector<ImageWindow*> result;
    for (ImageWindow* w : imageWindows_)
        if (w) result.append(w);
    return result;
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::openFile(const QString& path) {
    if (path.endsWith(".ply", Qt::CaseInsensitive)) {
        openPlyFile(path);
        return;
    }
    ViffReader reader;
    ViffImage img;
    if (!reader.load(path.toStdString(), img)) {
        QMessageBox::critical(this, "Load Error",
            QString("Cannot load VIFF file:\n%1\n\n%2")
                .arg(path)
                .arg(QString::fromStdString(reader.lastError())));
        return;
    }
    openImageWindow(std::move(img), path);
}

void MainWindow::openImageWindow(ViffImage img, const QString& title,
                                  const RoiMask* roiMask) {
    const int idx = nextIndex_++;
    auto* win = new ImageWindow(idx, title, std::move(img));
    win->setMainWindow(this);
    connect(win, &ImageWindow::windowClosing, this, &MainWindow::onImageWindowClosing);

    // Copy ROI mask if provided
    if (roiMask && !roiMask->isEmpty())
        win->setRoiMask(*roiMask);

    if (idx >= imageWindows_.size())
        imageWindows_.resize(idx + 1, nullptr);
    imageWindows_[idx] = win;

    addImageToLists(idx, QFileInfo(title).fileName());
    win->show();
}

void MainWindow::onOpenViff() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, "Open VIFF", lastDir_,
        "VIFF/XV Files (*.xv *.viff);;All Files (*)");
    for (const QString& path : paths) {
        lastDir_ = QFileInfo(path).absolutePath();
        openFile(path);
    }
}

void MainWindow::onOpenPly() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, "Open PLY", lastDir_,
        "PLY Files (*.ply);;All Files (*)");
    for (const QString& path : paths) {
        lastDir_ = QFileInfo(path).absolutePath();
        openPlyFile(path);
    }
}

void MainWindow::openPlyFile(const QString& path) {
    PlyIO::Probe probe;
    std::string err;
    if (!PlyIO::probe(path.toStdString(), probe, err)) {
        QMessageBox::critical(this, "PLY Import Error",
            QString("Cannot read PLY file:\n%1\n\n%2").arg(path).arg(QString::fromStdString(err)));
        return;
    }

    // Ask user to confirm / adjust pixel size
    QDialog dlg(this);
    dlg.setWindowTitle("PLY Import Parameters");
    auto* form = new QFormLayout(&dlg);

    form->addRow(new QLabel(QString("%1 points   X: [%2 … %3]   Y: [%4 … %5]")
        .arg(probe.pointCount)
        .arg(probe.xMin, 0, 'g', 5).arg(probe.xMax, 0, 'g', 5)
        .arg(probe.yMin, 0, 'g', 5).arg(probe.yMax, 0, 'g', 5)));

    auto* sbXps = new QDoubleSpinBox;
    sbXps->setRange(1e-9, 1e6);
    sbXps->setDecimals(8);
    sbXps->setValue(static_cast<double>(probe.xPixelSize));
    form->addRow("Pixel size X:", sbXps);

    auto* sbYps = new QDoubleSpinBox;
    sbYps->setRange(1e-9, 1e6);
    sbYps->setDecimals(8);
    sbYps->setValue(static_cast<double>(probe.yPixelSize));
    form->addRow("Pixel size Y:", sbYps);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    const float xps = static_cast<float>(sbXps->value());
    const float yps = static_cast<float>(sbYps->value());

    ViffImage img;
    if (!PlyIO::read(path.toStdString(), img, xps, yps, err)) {
        QMessageBox::critical(this, "PLY Import Error",
            QString("Failed to rasterize PLY:\n%1").arg(QString::fromStdString(err)));
        return;
    }

    openImageWindow(std::move(img), path);
}

void MainWindow::onCloseAll() {
    // Copy list since closing triggers onImageWindowClosing which modifies imageWindows_
    const QVector<ImageWindow*> wins = imageWindows_;
    for (ImageWindow* win : wins) {
        if (win) win->close();
    }
}

void MainWindow::onGlobalParameters() {
    GlobalParametersDialog dlg(settings_, this);
    dlg.exec();
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About Match3D+",
        "<h3>Match3D+</h3>"
        "<p>3D surface registration and wear analysis for dental research.</p>"
        "<p><b>Author:</b><br>"
        "Prof. Dr. Karl-Heinz Kunzelmann<br>"
        "LMU Munich</p>"
        "<p><b>Based on:</b><br>"
        "Match3D 2.5 by Wolfram Gloger</p>"
        "<p><b>Libraries:</b><br>"
        "Qt 6, CCCoreLib (CloudCompare)</p>"
        "<p><b>License:</b><br>"
        "GPL v2 or later</p>");
}

void MainWindow::onImage1SelectionChanged(QListWidgetItem* current) {
    if (!current) return;
    selectedIndex1_ = current->data(kIndexRole).toInt();
    raiseImageWindow(selectedIndex1_);
}

void MainWindow::onImage2SelectionChanged(QListWidgetItem* current) {
    if (!current) return;
    selectedIndex2_ = current->data(kIndexRole).toInt();
    raiseImageWindow(selectedIndex2_);
}

void MainWindow::onImageWindowClosing(int index) {
    if (index >= 0 && index < imageWindows_.size())
        imageWindows_[index] = nullptr;
    if (selectedIndex1_ == index) selectedIndex1_ = -1;
    if (selectedIndex2_ == index) selectedIndex2_ = -1;
    removeImageFromLists(index);
    emit imageWindowClosed(index);
}

QVector<ImageWindow*> MainWindow::selectedPair() const {
    ImageWindow* w1 = (selectedIndex1_ >= 0 && selectedIndex1_ < imageWindows_.size())
                      ? imageWindows_[selectedIndex1_] : nullptr;
    ImageWindow* w2 = (selectedIndex2_ >= 0 && selectedIndex2_ < imageWindows_.size())
                      ? imageWindows_[selectedIndex2_] : nullptr;
    if (w1 && w2)
        return {w1, w2};
    return imageWindows();  // fallback: both lists need a selection
}
