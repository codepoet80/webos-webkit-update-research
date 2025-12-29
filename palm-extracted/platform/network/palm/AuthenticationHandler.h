/*
 *  Copyright (C) 2008 Palm, Inc.  All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef AUTHENTICATIONHANDLER_H
#define AUTHENTICATIONHANDLER_H

#include <CString.h>
#include <PlatformString.h>

namespace WebCore
{

class KURL;
class CurlHandle;
class ResourceHandleInternal;
class Credential;
class CredentialsCache;
class CredentialEntry;
class ProtectionSpace;
class AuthenticationChallenge;

class AuthenticationHandler
{
public:

    AuthenticationHandler(CredentialsCache* cache, CurlHandle* job);
    ~AuthenticationHandler();

    void reset();
    bool cancelled() const { return m_cancelled; }

    // make sure to call this after all the custom headers have been set
    void addCredentials(const KURL& url, const String& username, const String& password);

    bool handleChallenge();

    void receivedUserCredentials(const AuthenticationChallenge& challenge,
                                 const Credential& credential);

    void receivedUserCancellation(const AuthenticationChallenge& challenge);

private:

    enum NtlmState {
        NtlmStateNone,
        NtlmStateType1,
        NtlmStateType3
    };

    bool handleBasicChallenge(const KURL& url, const char* challengeStr);
    bool handleDigestChallenge(const KURL& url, const char* challengeStr);
    bool handleNtlmChallenge(const KURL& url, const char* challengeStr);

    String genBasicResponse(const KURL& url, const CredentialEntry& entry);
    String genDigestResponse(const KURL& url, const CredentialEntry& entry);
    String genNtlmResponse(const KURL& url, const CredentialEntry& entry);
	static void getUrlDirectory(const KURL& url, String& dir);

    void removeAuthHeader();
    void addAuthHeader(const String& authHeader);

    CredentialsCache* m_cache;
    CurlHandle*       m_chandle;
    bool              m_cancelled;
    bool              m_realmBasedAuthHeaderAdded;
    bool              m_pathBasedAuthHeaderAdded;
    bool              m_authHeaderAddded;
    String            m_user;
    String            m_password;
    NtlmState         m_ntlmState;
};

}

#endif /* AUTHENTICATIONHANDLER_H */
