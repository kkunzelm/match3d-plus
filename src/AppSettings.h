#pragma once

#include <QSettings>

// Global parameters matching Gloger's match3d "Global Parameters" dialog.
// Single instance held by MainWindow, passed by pointer where needed.
struct AppSettings {
    float clipMin      = 0.0f;
    float clipMax      = 14000.0f;
    float clipGrad     = 4.0f;
    float clipDz       = 200.0f;
    float viewExp      = 1.5f;
    int   matchQuant   = 5;
    float scaleX       = 1.0f;
    float scaleY       = 1.0f;
    float scaleZ       = 1.0f;
    int   fpFitpixel   = 10;
    float fpHoehe      = 2000.0f;

    void load(QSettings& s);
    void save(QSettings& s) const;
};
