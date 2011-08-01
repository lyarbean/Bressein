// see http://www.qtcentre.org/wiki/index.php?title=Singleton_pattern
// This is thread-safe singleton template
#ifndef SINGLETON_H
#define SINGLETON_H

#include <QObject>
#include <QMutex>

template <class T>
class Singleton
{
public:
    static T *instance()
    {
        static QMutex mutex;
        if (not _instance)
        {
            mutex.lock();
            if (not _instance)
            {
                _instance = new T;
            }
            mutex.unlock();
        }
        return _instance;
    }
private:
    // requires c++0x or gnu++0x support
    Singleton() = delete;
    ~Singleton() = delete;
    Singleton (const Singleton &) = delete; // hide copy constructor
    Singleton &operator= (const Singleton &) = delete; // hide assign op
    static T *_instance;
};
template <class T>
T *Singleton<T>::_instance = 0;
#endif
