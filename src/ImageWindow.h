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
