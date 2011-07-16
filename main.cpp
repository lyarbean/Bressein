#include <QtGui/QApplication>
#include "Bressein.h"


int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Bressein foo;
    foo.show();
    return app.exec();
}
