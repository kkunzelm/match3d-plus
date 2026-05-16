#include "AppSettings.h"

void AppSettings::load(QSettings& s) {
    s.beginGroup("GlobalParameters");
    clipMin    = s.value("clipMin",    clipMin).toFloat();
    clipMax    = s.value("clipMax",    clipMax).toFloat();
    clipGrad   = s.value("clipGrad",   clipGrad).toFloat();
    clipDz     = s.value("clipDz",     clipDz).toFloat();
    viewExp    = s.value("viewExp",    viewExp).toFloat();
    matchQuant = s.value("matchQuant", matchQuant).toInt();
    scaleX     = s.value("scaleX",     scaleX).toFloat();
    scaleY     = s.value("scaleY",     scaleY).toFloat();
    scaleZ     = s.value("scaleZ",     scaleZ).toFloat();
    fpFitpixel = s.value("fpFitpixel", fpFitpixel).toInt();
    fpHoehe    = s.value("fpHoehe",    fpHoehe).toFloat();
    s.endGroup();
}

void AppSettings::save(QSettings& s) const {
    s.beginGroup("GlobalParameters");
    s.setValue("clipMin",    clipMin);
    s.setValue("clipMax",    clipMax);
    s.setValue("clipGrad",   clipGrad);
    s.setValue("clipDz",     clipDz);
    s.setValue("viewExp",    viewExp);
    s.setValue("matchQuant", matchQuant);
    s.setValue("scaleX",     scaleX);
    s.setValue("scaleY",     scaleY);
    s.setValue("scaleZ",     scaleZ);
    s.setValue("fpFitpixel", fpFitpixel);
    s.setValue("fpHoehe",    fpHoehe);
    s.endGroup();
}
