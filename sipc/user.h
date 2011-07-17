/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#ifndef BRESSEIN_USER_H
#define BRESSEIN_USER_H

#include <QtCore/QObject>
#include <QtNetwork/QTcpSocket>
#include "types.h"
namespace Bressein {
    // As SSi login is a short-connection, we don't run it in another thread
    // so for, to make the code work ASAP, we are not to provide robust multi threaded
    // a TCPsocket is required to perform sipc transactions
    // as well as a UDPsocket for file transferring.
    // we composite a Http proxy to handle some request.

/** \class User
 * @brief The User is an instance of a sip-c client.
 *
 **/
class User : public QObject
{
    Q_OBJECT
public:
    User(QByteArray number, QByteArray password);
    virtual ~User();
    virtual bool operator== (const User& other);

public slots:
    void setState(StateType&);
    void login();
    void keepAlive();//calls in period
    void sendMsg(QByteArray& fetionId, QByteArray& message);
    void addBuddy(QByteArray& number, QByteArray& info);
    //number could either be fetionId or phone number.
signals:
    void showStatus(QByteArray& message);// the code
    void serverConfigGot();
private:
    /**
     * @brief try to load local configuration and initialize info
     *
     * @return void
     **/
    void initialize(QByteArray number, QByteArray password);
    /**
     * @brief get server configuration
     *
     * @return void
     **/
    void getServerConfig();
    /**
     * @brief Ssi portal login
     *
     * @return void
     **/
    void ssiLogin();
    void handleSsiResponse(QByteArray data);
    void handleSipcRegisterResponse(QByteArray data);
    void handleSipcAuthorizeResponse (QByteArray data);
private slots:
    /**
     * @brief parse server configuration
     *
     * @param  ...
     * @return void
     **/
    void parseServerConfig(QByteArray data);
    void sipcRegister();
    void sipcAuthorize();
    void getSsiPic();
private:
    class  UserInfo;
    typedef UserInfo * Info;
    Info info;
    QList<contact *> contacts;
    // groups
    //pggroups
    QTcpSocket* sipcSocket; // All sip-c transactions are handle through this socket
};
}
#endif // USER_H
