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

extern QByteArray hashV1 (const QByteArray &userId, const QByteArray &password);

extern QByteArray hashV2 (const QByteArray &userId, const QByteArray &hashV4);

extern QByteArray hashV4 (const QByteArray &userId, const QByteArray &password);

extern QByteArray RSAPublicEncrypt (
        const QByteArray &userId,
        const QByteArray &password,
        const QByteArray &nonce,
        const QByteArray &aeskey,
        const QByteArray &key/*public key*/);

extern QByteArray cnouce (quint16 time = 4);

extern QByteArray configData (const QByteArray &number);

extern QByteArray ssiLoginData (
        const QByteArray &number,
        const QByteArray &passwordhashed4,
        const QByteArray passwordType = "1");

extern QByteArray SsiPicData (
        const QByteArray &algorithm, const QByteArray &ssic);

extern QByteArray ssiVerifyData (
        const QByteArray &number,
        const QByteArray &passwordhashed4,
        const QByteArray &guid,
        const QByteArray &code,
        const QByteArray &algorithm,
        QByteArray passwordType = "1");

extern QByteArray sipcAuthorizeData (
        const QByteArray &mobileNumber,
        const QByteArray &fetionNumber,
        const QByteArray &userId,
        int &callId,
        const QByteArray &response,
        const QByteArray &personalVersion,
        const QByteArray &customConfigVersion,
        const QByteArray &contactVersion,
        const QByteArray &state);

extern QByteArray keepAliveData (const QByteArray &fetionNumber, int& callId);

extern QByteArray messagedata (
        const QByteArray &fromFetionNumber,
        const QByteArray &toSipuri,
        int& callId,
        const QByteArray &message);

extern QByteArray addBuddyData (
        const QByteArray &fromFetionNumber,
        const QByteArray &buddyNumber,
        int &callId,
        const QByteArray &buddyLists,
        const QByteArray &localName,
        const QByteArray &desc,
        const QByteArray &phraseId);

extern QByteArray contactInfoData (
        const QByteArray &fetionNumber,
        const QByteArray &userId, int &callId);
}

/*! @} */
#endif

