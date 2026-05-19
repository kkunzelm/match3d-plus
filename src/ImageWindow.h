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

#include "io/ViffReader.h"
#include "registration/Transformation3D.h"
#include "RoiMask.h"

#include <QMainWindow>
#include <QString>

class QCheckBox;
class QComboBox;
class DepthImageView;
class QLabel;
class QRadioButton;
class MainWindow;
class MatchingControlPanel;

class ImageWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit ImageWindow(int index, const QString& path,
                         ViffImage image, QWidget* parent = nullptr);

    int imageIndex() const { return index_; }
    const QString& imagePath() const { return path_; }
    const ViffImage& image() const { return image_; }
    const RoiMask&   roiMask() const { return roiMask_; }
    void setRoiMask(const RoiMask& mask);
    DepthImageView* depthView() const { return depthView_; }

    void setMainWindow(MainWindow* mw) { mainWindow_ = mw; }

    // Display style enum used by DepthImageView
    enum class Style { Linear, FalseColor, MediumGray, Linear2, GrayCast };
    Style currentStyle() const { return style_; }

signals:
    void windowClosing(int index);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPolygonCompleted(QPolygonF poly, bool select);

private:
    void createMenus();
    void createToolBar();
    void createCentralWidget();
    void createStatusBar();

    // ROI helpers
    void applyRoiOp(std::function<void(RoiMask&)> op);
    void showStripDialog(bool horizontal, bool select);
    void showEllipseDialog(bool select);
    void showZClipDialog();

    // Match menu
    void showMatchingControlPanel();

    // Processing helpers
    void applyImgOp(std::function<void(ViffImage&)> op);
    void showShiftDialog();
    void showScaleDialog();
    void showStatisticsDialog();
    void showHistogramDialog();
    void showFitPlaneDialog();
    void showFitSphereDialog();

    int index_;
    QString path_;
    ViffImage image_;
    RoiMask roiMask_;
    Style style_ = Style::Linear;
    Transformation3D matchTransform_;

    // Saved on load for "Scale to original"
    float origXPixelSize_ = 0.0f;
    float origYPixelSize_ = 0.0f;

    // Toolbar widgets
    QComboBox*     styleCombo_  = nullptr;
    QRadioButton*  radioAll_    = nullptr;
    QRadioButton*  radioRoi_    = nullptr;
    QCheckBox*     crosshCheck_ = nullptr;

    // Central view
    DepthImageView* depthView_  = nullptr;

    // Status bar label
    QLabel* coordLabel_ = nullptr;

    // Back-reference to main window (for matching panel target list)
    MainWindow*           mainWindow_     = nullptr;
    MatchingControlPanel* matchingPanel_  = nullptr;
};
