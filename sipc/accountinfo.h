/*
This file is part of Bressein.
Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>

Bressein is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

Bressein is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

OpenSSL linking exception
--------------------------
If you modify this Program, or any covered work, by linking or
combining it with the OpenSSL project's "OpenSSL" library (or a
modified version of that library), containing parts covered by
the terms of OpenSSL/SSLeay license, the licensors of this
Program grant you additional permission to convey the resulting
work. Corresponding Source for a non-source form of such a
combination shall include the source code for the parts of the
OpenSSL library used as well as that of the covered work.
*/

#ifndef BRESSEIN_USER_PRIVATE_H
#define BRESSEIN_USER_PRIVATE_H
#include "account.h"

namespace Bressein
{

struct Account::Info
{
    Info (Account *parent)
    {
        _p = parent;
    }

    // from input
    QByteArray loginNumber;  // fetion number or mobile phone number
    QByteArray mobileNumber;
    QByteArray fetionNumber; // sId
    QByteArray password; //raw
    int callId; // initialized as 1
    QByteArray cnonce; // CN's value

    // from Ssi
    QByteArray userId;       // user id
    QByteArray sipuri;       // sipuri like 'sip:100@fetion.com.cn'
    QByteArray ssic;         //cookie string
    QByteArray credential;
    // the followings are got immediately after sipc'd successfully
    // from Sip-c reg, seems could be removed
    QByteArray nonce;
    QByteArray key;
    QByteArray signature;
    // to validate, seems could be removed
    QByteArray aeskey;     // random?
    QByteArray response;
    QByteArray pgGroupCallId_;    // callid for get group list request
    QByteArray groupInfoCallId;   // callid for get group info request
    // for local use
    QByteArray state; // presence state
    QByteArray loginStatus;       // login status code
    QByteArray portraitCrc;// a number generated by CRC algorithm
//         int contactCount;
//         int groupCount;

    //custom config string used to set personal information

    struct Client
    {
        QByteArray publicIp;     // public ip of current session
        QByteArray lastLoginIp;  // public ip of last login
        QByteArray lastLoginTime;
        // last login time , got after sipc authentication
        QByteArray loginTimes;
        QByteArray nickname;
        QByteArray version;    // personalVersion;
        QByteArray sId;       //  the same as fetionNumber if got
        QByteArray mobileNo;
        // the same as mobileNumber, if present means boundToMobile?? odd
        QByteArray carrierStatus;
        QByteArray gender;
        QByteArray smsOnLineStatus;
        QByteArray impresa;
        QByteArray carrierRegion; // country.province.city
        QByteArray birthDate;
        QByteArray contactVersion;
        QByteArray customConfig;
        QByteArray customConfigVersion;
        QByteArray smsDayLimit;
        QByteArray smsDayCount;
        QByteArray smsMonthLimit;
        QByteArray smsMonthCount;
    } client;

    struct Verfication
    {
        QByteArray algorithm;
        QByteArray type;
        QByteArray text;
        QByteArray tips;
        QByteArray code;
        QByteArray id;
        QByteArray pic;
    } verification; // initialized when needed

    struct SystemConfig
    {
        QByteArray serverVersion;
        QByteArray parametersVersion;
        QByteArray hintsVersion;
        QByteArray proxyIpPort;
        QByteArray serverNamePath; //get-uri
        QByteArray portraitServerName;
        QByteArray portraitServerPath;
    } systemconfig;

    QList<QByteArray> phrases;

protected:
    Account *_p;
};
}
#endif