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

#pragma once

#include <QMainWindow>
#include <QVector>
#include "AppSettings.h"
#include "io/ViffReader.h"
#include "RoiMask.h"

class QCloseEvent;
class QListWidget;
class QListWidgetItem;
class ImageWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Open a VIFF or PLY file; dispatches by extension (used by CLI argument loading)
    void openFile(const QString& path);

    // Open a computed (in-memory) image as a new window
    // If roiMask is provided, it will be copied to the new window's ROI mask
    void openImageWindow(ViffImage img, const QString& title,
                         const RoiMask* roiMask = nullptr);

    // Returns all currently open (non-null) image windows
    QVector<ImageWindow*> imageWindows() const;

    // Returns the two images selected in list1 / list2 for matching.
    // Falls back to all open windows when fewer than two images are selected.
    QVector<ImageWindow*> selectedPair() const;

signals:
    void imageWindowClosed(int index);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onOpenViff();
    void onOpenPly();
#ifdef MATCH3D_STL_IMPORT_ENABLED
    void onOpenStl();
#endif
    void onCloseAll();
    void onGlobalParameters();
    void onAbout();

    void onImage1SelectionChanged(QListWidgetItem* current);
    void onImage2SelectionChanged(QListWidgetItem* current);

    void onImageWindowClosing(int index);

private:
    void openPlyFile(const QString& path);  // shows pixel-size dialog then imports

    void createActions();
    void createMenus();
    void createCentralWidget();

    void addImageToLists(int index, const QString& name);
    void removeImageFromLists(int index);
    void raiseImageWindow(int index);

    // All open image windows (nullptr slot = closed window)
    QVector<ImageWindow*> imageWindows_;
    int nextIndex_ = 0;

    // Image-1 = baseline/reference, Image-2 = data/follow-up.
    QListWidget* imageList1_ = nullptr;
    QListWidget* imageList2_ = nullptr;

    // Indices of the images currently selected in each list (-1 = none selected).
    int selectedIndex1_ = -1;
    int selectedIndex2_ = -1;

    AppSettings settings_;
    QString lastDir_;

    // Actions
    QAction* actOpenViff_          = nullptr;
    QAction* actOpenPly_           = nullptr;
#ifdef MATCH3D_STL_IMPORT_ENABLED
    QAction* actOpenStl_           = nullptr;
#endif
    QAction* actCloseAll_          = nullptr;
    QAction* actQuit_              = nullptr;
    QAction* actGlobalParams_      = nullptr;
    QAction* actSetClipFromImg1_   = nullptr;
    QAction* actTransferPolygon_   = nullptr;
    QAction* actExtend1to2_        = nullptr;
    QAction* actMerge1in2_         = nullptr;
    QAction* actDiff2D_            = nullptr;
};
