#include "stdafx.h"
#include "UlteriusDemo.h"

// create a generic Qt application
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("Ultrasonix");
    app.setApplicationName("Ulterius Demo");

    QResource::registerResource("ulterius.rcc");

    UlteriusDemo demo;
    demo.setWindowTitle("Ulterius Demo");
    demo.show();

    return app.exec();
}
