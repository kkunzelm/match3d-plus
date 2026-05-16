#pragma once

#include <QDialog>
#include "../AppSettings.h"

class QLineEdit;

class GlobalParametersDialog : public QDialog {
    Q_OBJECT
public:
    explicit GlobalParametersDialog(AppSettings& settings, QWidget* parent = nullptr);

private slots:
    void onUpdate();
    void onRevert();

private:
    void populateFields();
    void applyToSettings();

    AppSettings& settings_;
    AppSettings  saved_;   // snapshot for Revert

    QLineEdit* leClipMin_    = nullptr;
    QLineEdit* leClipMax_    = nullptr;
    QLineEdit* leClipGrad_   = nullptr;
    QLineEdit* leClipDz_     = nullptr;
    QLineEdit* leViewExp_    = nullptr;
    QLineEdit* leMatchQuant_ = nullptr;
    QLineEdit* leScaleX_     = nullptr;
    QLineEdit* leScaleY_     = nullptr;
    QLineEdit* leScaleZ_     = nullptr;
    QLineEdit* leFpFitpixel_ = nullptr;
    QLineEdit* leFpHoehe_    = nullptr;
};
