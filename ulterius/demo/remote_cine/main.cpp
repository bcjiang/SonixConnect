#include "stdafx.h"
#include "RemoteCine.h"

// create a generic Qt application
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("Ultrasonix");
    app.setApplicationName("Remote Cine Storage");

    QResource::registerResource("ulterius.rcc");

    RemoteCine demo;
    demo.setWindowTitle("Remote Cine Storage");
    demo.show();

    return app.exec();
}
