#include "GlobalParametersDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

GlobalParametersDialog::GlobalParametersDialog(AppSettings& settings, QWidget* parent)
    : QDialog(parent)
    , settings_(settings)
    , saved_(settings)
{
    setWindowTitle("Global Parameters");
    setFixedWidth(260);

    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto makeEdit = [&](const QString& val) {
        auto* le = new QLineEdit(val);
        le->setMaximumWidth(120);
        return le;
    };

    leClipMin_    = makeEdit(QString::number(settings_.clipMin));
    leClipMax_    = makeEdit(QString::number(settings_.clipMax));
    leClipGrad_   = makeEdit(QString::number(settings_.clipGrad));
    leClipDz_     = makeEdit(QString::number(settings_.clipDz));
    leViewExp_    = makeEdit(QString::number(settings_.viewExp));
    leMatchQuant_ = makeEdit(QString::number(settings_.matchQuant));
    leScaleX_     = makeEdit(QString::number(settings_.scaleX));
    leScaleY_     = makeEdit(QString::number(settings_.scaleY));
    leScaleZ_     = makeEdit(QString::number(settings_.scaleZ));
    leFpFitpixel_ = makeEdit(QString::number(settings_.fpFitpixel));
    leFpHoehe_    = makeEdit(QString::number(settings_.fpHoehe));

    form->addRow("clip_min",     leClipMin_);
    form->addRow("clip_max",     leClipMax_);
    form->addRow("clip_grad",    leClipGrad_);
    form->addRow("clip_dz",      leClipDz_);
    form->addRow("view_exp",     leViewExp_);
    form->addRow("match_quant",  leMatchQuant_);
    form->addRow("scale_x",      leScaleX_);
    form->addRow("scale_y",      leScaleY_);
    form->addRow("scale_z",      leScaleZ_);
    form->addRow("FP_fitpixel",  leFpFitpixel_);
    form->addRow("FP_hoehe",     leFpHoehe_);

    // Buttons: OK, Update, Revert, Cancel  (like original)
    auto* btnBox   = new QDialogButtonBox;
    auto* btnOk     = btnBox->addButton("OK",     QDialogButtonBox::AcceptRole);
    auto* btnUpdate = btnBox->addButton("Update",  QDialogButtonBox::ApplyRole);
    auto* btnRevert = btnBox->addButton("Revert",  QDialogButtonBox::ResetRole);
    btnBox->addButton("Cancel", QDialogButtonBox::RejectRole);

    connect(btnOk,     &QPushButton::clicked, this, [this]{ applyToSettings(); accept(); });
    connect(btnUpdate, &QPushButton::clicked, this, &GlobalParametersDialog::onUpdate);
    connect(btnRevert, &QPushButton::clicked, this, &GlobalParametersDialog::onRevert);
    connect(btnBox,    &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(btnBox);
}

void GlobalParametersDialog::onUpdate() {
    applyToSettings();
}

void GlobalParametersDialog::onRevert() {
    settings_ = saved_;
    populateFields();
}

void GlobalParametersDialog::populateFields() {
    leClipMin_->setText(QString::number(settings_.clipMin));
    leClipMax_->setText(QString::number(settings_.clipMax));
    leClipGrad_->setText(QString::number(settings_.clipGrad));
    leClipDz_->setText(QString::number(settings_.clipDz));
    leViewExp_->setText(QString::number(settings_.viewExp));
    leMatchQuant_->setText(QString::number(settings_.matchQuant));
    leScaleX_->setText(QString::number(settings_.scaleX));
    leScaleY_->setText(QString::number(settings_.scaleY));
    leScaleZ_->setText(QString::number(settings_.scaleZ));
    leFpFitpixel_->setText(QString::number(settings_.fpFitpixel));
    leFpHoehe_->setText(QString::number(settings_.fpHoehe));
}

void GlobalParametersDialog::applyToSettings() {
    settings_.clipMin    = leClipMin_->text().toFloat();
    settings_.clipMax    = leClipMax_->text().toFloat();
    settings_.clipGrad   = leClipGrad_->text().toFloat();
    settings_.clipDz     = leClipDz_->text().toFloat();
    settings_.viewExp    = leViewExp_->text().toFloat();
    settings_.matchQuant = leMatchQuant_->text().toInt();
    settings_.scaleX     = leScaleX_->text().toFloat();
    settings_.scaleY     = leScaleY_->text().toFloat();
    settings_.scaleZ     = leScaleZ_->text().toFloat();
    settings_.fpFitpixel = leFpFitpixel_->text().toInt();
    settings_.fpHoehe    = leFpHoehe_->text().toFloat();
}
