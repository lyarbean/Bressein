#ifndef BRESSIEN_UTILS_H
#define BRESSIEN_UTILS_H

/*! \defgroup utils "Utils"
*  utils provide public declarations and functions.
*  @{
* */
#include <QByteArray>
namespace Bressein
{
/**
 * @brief The current Fetion's version
 **/
const static QByteArray PROTOCOL_VERSION = "4.3.0980";
const static QByteArray NAVIGATION = "nav.fetion.com.cn";
const static QByteArray DOMAIN = "fetion.com.cn";
const static QByteArray UID_URI = "uid.fetion.com.cn";

// Connection relative
extern QByteArray hashV1 (const QByteArray &userId, const QByteArray &password);

extern QByteArray hashV2 (const QByteArray &userId, const QByteArray &hashV4);

extern QByteArray hashV4 (const QByteArray &userId, const QByteArray &password);

extern QByteArray RSAPublicEncrypt (const QByteArray &userId,
                                    const QByteArray &password,
                                    const QByteArray &nonce,
                                    const QByteArray &aeskey,
                                    const QByteArray &key/*public key*/);

extern QByteArray cnouce (quint16 time = 4);

extern QByteArray configData (const QByteArray &number);

extern QByteArray ssiLoginData (const QByteArray &number,
                                const QByteArray &passwordhashed4,
                                const QByteArray passwordType = "1");

extern QByteArray SsiPicData (const QByteArray &algorithm,
                              const QByteArray &ssic);

extern QByteArray ssiVerifyData (const QByteArray &number,
                                 const QByteArray &passwordhashed4,
                                 const QByteArray &id,
                                 const QByteArray &code,
                                 const QByteArray &algorithm,
                                 QByteArray passwordType = "2");

extern QByteArray sipcAuthorizeData (const QByteArray &mobileNumber,
                                     const QByteArray &fetionNumber,
                                     const QByteArray &userId,
                                     int &callId,
                                     const QByteArray &response,
                                     const QByteArray &personalVersion,
                                     const QByteArray &customConfigVersion,
                                     const QByteArray &contactVersion,
                                     const QByteArray &state);

extern QByteArray keepAliveData (const QByteArray &fetionNumber, int &callId);

// Contact/Buddy relative
extern QByteArray catMsgData (const QByteArray &fromFetionNumber,
                              const QByteArray &toSipuri,
                              int &callId,
                              const QByteArray &message);

extern QByteArray sendCatMsgSelfData (const QByteArray &fetionNumber,
                                      const QByteArray &sipuri,
                                      int &callId,
                                      const QByteArray &message);

extern QByteArray sendCatMsgPhoneData (const QByteArray &fetionNumber,
                                       const QByteArray &sipuri,
                                       int &callId,
                                       const QByteArray &message);

extern QByteArray addBuddyV4Data (const QByteArray &fromFetionNumber,
                                  const QByteArray &buddyNumber,
                                  int &callId,
                                  const QByteArray &buddyLists,
                                  const QByteArray &localName,
                                  const QByteArray &desc,
                                  const QByteArray &phraseId);

extern QByteArray contactInfoData (const QByteArray &fetionNumber,
                                   const QByteArray &userId,
                                   int &callId);
extern QByteArray contactInfoData (const QByteArray &fetionNumber,
                                   const QByteArray &userId,
                                   int &callId,
                                   bool mobile);

extern QByteArray deleteBuddyV4Data (const QByteArray &fromFetionNumber,
                                     const QByteArray &userId,
                                     int &callId);

extern QByteArray presenceV4Data (const QByteArray &fetionNumber,
                                  int &callId);

extern QByteArray startChatData (const QByteArray &fetionNumber, int &callId);

extern QByteArray registerData (const QByteArray &fetionNumber,
                                int &callId,
                                const QByteArray &credential);

extern QByteArray inviteBuddyData (const QByteArray &fromFetionNumber,
                                   int &callId,
                                   const QByteArray &toSipuri);

extern QByteArray SetContactInfoV4 (const QByteArray &fromFetionNumber,
                                    const QByteArray &userId,
                                    int &callId,
                                    const bool show);

extern QByteArray SetContactInfoV4 (const QByteArray &fromFetionNumber,
                                    const QByteArray &userId,
                                    int &callId,
                                    const QByteArray &name);
//FIXME what format the group is? int?
extern QByteArray SetContactInfoV4 (const QByteArray &fromFetionNumber,
                                    const QByteArray &userId,
                                    int &callId,
                                    const int &group);
// Group
extern QByteArray createBuddyListData (const QByteArray &fetionNumber,
                                       int &callId,
                                       const QByteArray &name);

extern QByteArray deleteBuddyListData (const QByteArray &fetionNumber,
                                       int &callId,
                                       const QByteArray &id);

extern QByteArray setBuddyListInfoData (const QByteArray &fetionNumber,
                                        int &callId,
                                        const QByteArray &id,
                                        const QByteArray &name);

//

extern QByteArray handleContactRequestV4Data (const QByteArray &fetionNumber,
                                              int &callId,
                                              const QByteArray &userId,
                                              const QByteArray &sipUri,
                                              const QByteArray &localName,
                                              const QByteArray &buddyLists,
                                              const QByteArray &result);


extern QByteArray directSMSData (const QByteArray &fetionNumber,
                                 int &callId,
                                 const QByteArray &algorithm,
                                 const QByteArray &type,
                                 const QByteArray &id,
                                 const QByteArray &response);

extern QByteArray sendDirectCatSMSData (const QByteArray &fetionNumber,
                                        int &callId,
                                        const QByteArray &mobile,
                                        const QByteArray &message);
// Pg group
extern QByteArray pgGetGroupListData (const QByteArray &fetionNumber,
                                      int &callId);

extern QByteArray pgGetGroupInfoData (const QByteArray &fetionNumber,
                                      int &callId,
                                      const QList<QByteArray> &pguris);

extern QByteArray pgPresenceData (const QByteArray &fetionNumber,
                                  int &callId,
                                  const QByteArray &pguri);

extern QByteArray pgGetGroupMembersData (const QByteArray &fetionNumber,
                                         int &callId,
                                         const QByteArray &pguri);

extern QByteArray pgSendCatSMSData (const QByteArray &fetionNumber,
                                    int &callId,
                                    const QByteArray &pguri,
                                    const QByteArray &message);

// User per se.
// overloading
extern QByteArray setUserInfoV4Data (const QByteArray &fetionNumber,
                                     int &callId,
                                     const QByteArray &impresa,
                                     const QByteArray &nickName,
                                     const QByteArray &gender,
                                     const QByteArray &customConfig,
                                     const QByteArray &customConfigVersion);

extern QByteArray setUserInfoV4Data (const QByteArray &fetionNumber,
                                     int &callId,
                                     const QByteArray &impresa,
                                     const QByteArray &personalVersion,
                                     const QByteArray &customConfig,
                                     const QByteArray &customConfigVersion);

extern QByteArray setUserInfoV4Data (const QByteArray &fetionNumber,
                                     int &callId, const int days);

extern QByteArray setPresenceV4Data (const QByteArray &fetionNumber,
                                     int &callId,
                                     const QByteArray &state);
// extern QByteArray conversationData
}
/*! @} */
#endif
