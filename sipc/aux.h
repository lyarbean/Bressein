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

#ifndef BRESSIEN_AUX_H
#define BRESSIEN_AUX_H

/*! \defgroup utils "Utils"
*  utils provide public declarations and functions.
*  @{
* */
#include <QtCore/QByteArray>
namespace Bressein
{
/**
 * @brief The current Fetion's version
 **/
const static QByteArray PROTOCOL_VERSION = "4.5.0900";
const static QByteArray NAVIGATION = "nav.fetion.com.cn";
const static QByteArray DOMAIN_URI = "fetion.com.cn";
const static QByteArray UID_URI = "uid.fetion.com.cn";

extern const QByteArray sipToFetion (const QByteArray &);

extern const QByteArray hashV1 (const QByteArray &userId,
                                const QByteArray &password);

extern const QByteArray hashV2 (const QByteArray &userId,
                                const QByteArray &hashV4);

extern const QByteArray hashV4 (const QByteArray &userId,
                                const QByteArray &password);

extern void RSAPublicEncrypt (const QByteArray &userId,
                              const QByteArray &password,
                              const QByteArray &nonce,
                              const QByteArray &aeskey,
                              const QByteArray &key,
                              QByteArray &result,
                              bool &ok);

extern const QByteArray cnouce (quint16 time = 4);

extern const QByteArray configData (QByteArray number = "13910000000");

extern const QByteArray SsiPicData (const QByteArray &algorithm,
                                    const QByteArray &ssic);

extern const QByteArray ssiData (const QByteArray &number,
                                 const QByteArray &passwordhashed4,
                                 const QByteArray &id,
                                 const QByteArray &code,
                                 const QByteArray &algorithm);

extern const QByteArray downloadPortraitData (const QByteArray &portraitName,
                                              const QByteArray &portraitPath,
                                              const QByteArray &sipuri,
                                              const QByteArray &ssic);

extern const QByteArray downloadPortraitAgainData (const QByteArray &path,
                                                   const QByteArray &host);
extern const QByteArray sipcAuthorizeData (const QByteArray &mobileNumber,
                                           const QByteArray &fetionNumber,
                                           const QByteArray &userId,
                                           int &callId,
                                           const QByteArray &response,
                                           const QByteArray &personalVersion,
                                           const QByteArray &customConfigVer,
                                           const QByteArray &contactVersion,
                                           const QByteArray &state,
                                           const QByteArray &ackData);

extern const QByteArray sipcAckData (const QByteArray &number,
                                     const QByteArray &passwordhashed4,
                                     const QByteArray &type,
                                     const QByteArray &id,
                                     const QByteArray &code,
                                     const QByteArray &algorithm);

extern const QByteArray keepAliveData (const QByteArray &fetionNumber,
                                       int &callId);

// a plain sip-c header
extern const QByteArray chatKeepAliveData (const QByteArray &fetionNumber,
                                           int &callId);
// Contact/Buddy relative
extern const QByteArray catMsgData (const QByteArray &fromFetionNumber,
                                    const QByteArray &toSipuri,
                                    int &callId,
                                    const QByteArray &message);

extern const QByteArray sendCatMsgSelfData (const QByteArray &fetionNumber,
                                            const QByteArray &sipuri,
                                            int &callId,
                                            const QByteArray &message);

extern const QByteArray sendCatMsgPhoneData (const QByteArray &fromFetionNumber,
                                             const QByteArray &toSipuri,
                                             int &callId,
                                             const QByteArray &message,
                                             const QByteArray &id,
                                             const QByteArray &code);

extern const QByteArray addBuddyV4Data (const QByteArray &fromFetionNumber,
                                        const QByteArray &buddyNumber,
                                        int &callId,
                                        const QByteArray &buddyLists,
                                        const QByteArray &localName,
                                        const QByteArray &desc,
                                        const QByteArray &phraseId);

extern const QByteArray contactInfoData (const QByteArray &fetionNumber,
                                         const QByteArray &userId,
                                         int &callId);

extern const QByteArray contactInfoData (const QByteArray &fetionNumber,
                                         const QByteArray &userId,
                                         int &callId,
                                         bool mobile);

extern const QByteArray deleteBuddyV4Data (const QByteArray &fromFetionNumber,
                                           const QByteArray &userId,
                                           int &callId);

extern const QByteArray presenceV4Data (const QByteArray &fetionNumber,
                                        int &callId);

extern const QByteArray startChatData (const QByteArray &fetionNumber,
                                       int &callId);

extern const QByteArray registerData (const QByteArray &fetionNumber,
                                      int &callId,
                                      const QByteArray &credential);

extern const QByteArray inviteBuddyData (const QByteArray &fromFetionNumber,
                                         int &callId,
                                         const QByteArray &toSipuri);

extern const QByteArray SetContactInfoV4 (const QByteArray &fromFetionNumber,
                                          const QByteArray &userId,
                                          int &callId,
                                          const bool show);

extern const QByteArray SetContactInfoV4 (const QByteArray &fromFetionNumber,
                                          const QByteArray &userId,
                                          int &callId,
                                          const QByteArray &name);
//FIXME what format the group is? int?
extern const QByteArray SetContactInfoV4 (const QByteArray &fromFetionNumber,
                                          const QByteArray &userId,
                                          int &callId,
                                          const int &group);
// Group
extern const QByteArray createBuddyListData (const QByteArray &fetionNumber,
                                             int &callId,
                                             const QByteArray &name);

extern const QByteArray deleteBuddyListData (const QByteArray &fetionNumber,
                                             int &callId,
                                             const QByteArray &id);

extern const QByteArray setBuddyListInfoData (const QByteArray &fetionNumber,
                                              int &callId,
                                              const QByteArray &id,
                                              const QByteArray &name);

extern const QByteArray handleContactRequestV4Data
(const QByteArray &fetionNumber,
 int &callId,
 const QByteArray &userId,
 const QByteArray &sipuri,
 const QByteArray &localName,
 const QByteArray &buddyLists,
 const QByteArray &result);


extern const QByteArray directSMSData (const QByteArray &fetionNumber,
                                       int &callId,
                                       const QByteArray &algorithm,
                                       const QByteArray &type,
                                       const QByteArray &id,
                                       const QByteArray &response);

extern const QByteArray sendDirectCatSMSData (const QByteArray &fetionNumber,
                                              int &callId,
                                              const QByteArray &mobile,
                                              const QByteArray &message);
// Pg group
extern const QByteArray pgGetGroupListData (const QByteArray &fetionNumber,
                                            int &callId);

extern const QByteArray pgGetGroupInfoData (const QByteArray &fetionNumber,
                                            int &callId,
                                            const QList<QByteArray> &pguris);

extern const QByteArray pgPresenceData (const QByteArray &fetionNumber,
                                        int &callId,
                                        const QByteArray &pguri);

extern const QByteArray pgGetGroupMembersData (const QByteArray &fetionNumber,
                                               int &callId,
                                               const QByteArray &pguri);

extern const QByteArray pgSendCatSMSData (const QByteArray &fetionNumber,
                                          int &callId,
                                          const QByteArray &pguri,
                                          const QByteArray &message);

// User per se.
// overloading
extern const QByteArray setUserInfoV4Data (const QByteArray &fetionNumber,
                                           int &callId,
                                           const QByteArray &impresa,
                                           const QByteArray &nickName,
                                           const QByteArray &gender,
                                           const QByteArray &customConfig,
                                           const QByteArray &customConfigVer);

extern const QByteArray setUserInfoV4Data (const QByteArray &fetionNumber,
                                           int &callId,
                                           const QByteArray &impresa,
                                           const QByteArray &personalVersion,
                                           const QByteArray &customConfig,
                                           const QByteArray &customConfigVer);

extern const QByteArray setUserInfoV4Data (const QByteArray &fetionNumber,
                                           int &callId, const int days);

extern const QByteArray setPresenceV4Data (const QByteArray &fetionNumber,
                                           int &callId,
                                           const QByteArray &state);
extern const QByteArray inviteAckData (const QByteArray &fetionNumber,
                                       const QByteArray &sipuri,
                                       const QByteArray &callId);
//FIXME
// extern const QByteArray acceptTransferV4(const QByteArray &fetionNumber,
//                         const QByteArray );
// extern const QByteArray conversationData
}
/*! @} */
#endif // BRESSIEN_AUX_H
