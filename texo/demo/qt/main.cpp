#include "stdafx.h"
#include "TexoDemo.h"

// create a generic Qt application
void setTheme(QApplication* app, QString themeName);

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("Ultrasonix");
    app.setApplicationName("Texo Demo");
    setTheme(&app, "Dark");
    QResource::registerResource("texo.rcc");

    TexoDemo demo;
    demo.setWindowTitle("Texo Demo");
    demo.show();

    return app.exec();
}

void setTheme(QApplication* app, QString themeName)
{
    if (themeName == "Dark")
    {
        QString filepath = ":/stylesheets/dark.qss";

        QFile file(filepath);

        if (file.open(QFile::ReadOnly))
        {
            QString styleSheet = QLatin1String(file.readAll());

            file.close();

            app->setStyleSheet(styleSheet);
        }
    }
    else if (themeName == "Classic")
    {
        app->setStyleSheet("");
    }
}
