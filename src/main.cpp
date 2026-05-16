#include <QApplication>
#include <QMetaType>
#include <QTimer>
#include "MainWindow.h"
#include "registration/Transformation3D.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    qRegisterMetaType<Transformation3D>();
    app.setApplicationName("match3d_v2");
    app.setApplicationVersion("0.1");
    app.setOrganizationName("Kunzelmann");

    MainWindow w;
    w.show();

    // Open files passed as command-line arguments
    const QStringList args = app.arguments().mid(1);
    if (!args.isEmpty()) {
        QTimer::singleShot(0, &w, [&w, args] {
            for (const QString& path : args)
                w.openFile(path);
        });
    }

    return app.exec();
}
