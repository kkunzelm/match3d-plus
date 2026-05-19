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

#include <QApplication>
#include <QMetaType>
#include <QTimer>
#include "MainWindow.h"
#include "registration/Transformation3D.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    qRegisterMetaType<Transformation3D>();
    app.setApplicationName("Match3D+");
    app.setApplicationVersion("1.0");
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
