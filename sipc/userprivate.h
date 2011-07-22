#ifndef BRESSEIN_USER_PRIVATE_H
#define BRESSEIN_USER_PRIVATE_H
#include "user.h"

namespace Bressein
{

    struct User::UserInfo
    {
        UserInfo (User * parent)
        {
            _p = parent;
        }

        virtual ~UserInfo()
        {
            if (verfication)
                delete verfication;
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
        QByteArray customConfig;//custom config string used to set personal information

        struct Client
        {
            QByteArray publicIp;     // public ip of current session
            QByteArray lastLoginIp;  // public ip of last login
            QByteArray lastLoginTime;// last login time , got after sipc authentication
            QByteArray loginTimes;
            QByteArray nickname;
            QByteArray version;    // personalVersion;
            QByteArray sId;       //  the same as fetionNumber if got
            QByteArray mobileNo; // the same as mobileNumber, if present means boundToMobile?? odd
            QByteArray carrierStatus;
            QByteArray gender;
            QByteArray smsOnLineStatus;
            QByteArray impresa;
            QByteArray carrierRegion; // country.province.city
            QByteArray birthDate;
            QByteArray contactVersion;
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
            QByteArray guid;
        } *verfication; // initialized when needed

        struct SystemConfig
        {
            QByteArray serverVersion;
            QByteArray parametersVersion;
            QByteArray hintsVersion;
            QByteArray proxyIpPort;
            QByteArray serverNamePath; //get-uri
        } systemconfig;

        QList<QByteArray> phrases;

    protected:
        User * _p;
    };
}
#endif