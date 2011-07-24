#ifndef Bressein_H
#define Bressein_H

#include <QtGui/QGraphicsScene>
#include "sipc/user.h"
// This is for test, will be rewritten with Qt graphics view framework
// and will be moved to gui;

class QThread;

namespace Bressein
{

class Bressein : public QGraphicsScene
{
    Q_OBJECT

public:
    Bressein();
    virtual ~Bressein();
    User * user;
};
}
#endif // Bressein_H
