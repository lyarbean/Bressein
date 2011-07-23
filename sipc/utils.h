#ifndef BRESSIEN_UTILS_H
#define BRESSIEN_UTILS_H
#include <QDateTime>
#include <QCryptographicHash>
#include <openssl/rsa.h>
#include <QDebug>
/*! \defgroup utils "Utils"
*  utils provide public declarations and functions.
*  @{
* */

namespace Bressein
{
    /**
     * @brief The current Fetion's version
     **/
    const static QByteArray PROTOCOL_VERSION = "4.3.0980";
    const static QByteArray NAVIGATION = "nav.fetion.com.cn";
    const static QByteArray DOMAIN = "fetion.com.cn";
    const static QByteArray UID_URI = "uid.fetion.com.cn";

    extern  QByteArray hashV1 (QByteArray &userId, QByteArray &password);

    extern QByteArray hashV2 (QByteArray &userId, QByteArray &hashV4);

    extern QByteArray hashV4 (QByteArray &userId, QByteArray &password);

    extern QByteArray RSAPublicEncrypt (QByteArray userId,
                                        QByteArray password,
                                        QByteArray nonce,
                                        QByteArray aeskey,
                                        QByteArray key/*public key*/);

    extern QByteArray cnouce (quint16 time = 4);

    extern QByteArray configData (QByteArray& number);

    extern QByteArray ssiLoginData (QByteArray& number,
                                    QByteArray& passwordhashed4,
                                    QByteArray passwordType = "1");

    extern QByteArray SsiPicData (QByteArray algorithm, QByteArray ssic);

    extern QByteArray ssiVerifyData (QByteArray& number,
                                     QByteArray& passwordhashed4,
                                     QByteArray& guid,
                                     QByteArray& code,
                                     QByteArray& algorithm,
                                     QByteArray passwordType = "1");

    extern QByteArray sipcAuthorizeBody (QByteArray& mobileNumber,
                                         QByteArray& userId,
                                         QByteArray& personalVersion,
                                         QByteArray& customConfigVersion,
                                         QByteArray& contactVersion,
                                         QByteArray& state);

    extern QByteArray keepAliveData (QByteArray &fetionNumber, int& callId);

    extern QByteArray messagedata (QByteArray &fromFetionNumber, QByteArray &toSipuri, int& callId, QByteArray& message);

    extern QByteArray addBuddyData (QByteArray& fromFetionNumber, QByteArray& buddyNumber,
                                    int& callId, QByteArray& buddyLists, QByteArray& localName,
                                    QByteArray& desc,  QByteArray& phraseId);

    extern QByteArray contactInfoData (QByteArray& fetionNumber, QByteArray &userId, int& callId);
}

/*! @} */
#endif
