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

#ifndef CREDENTIALSCACHE_H
#define CREDENTIALSCACHE_H

#include <PlatformString.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <StringHash.h>
#include <CString.h>

#include <ProtectionSpace.h>

namespace WebCore
{

class CredentialEntry
{
public:

    CredentialEntry();
    CredentialEntry(const CredentialEntry& e);
    CredentialEntry& operator=(const CredentialEntry& e);
    
    ProtectionSpaceAuthenticationScheme scheme;
    String protocol;
    String host;
    short  port;
    String path;
    String realm;
    String user;
    String password;
    String digestNonce;
    String digestQop;
    String digestAlgorithm;
    String digestOpaque;
    unsigned int ntlmFlags;
    unsigned char ntlmNonce[8];
};

class CredentialsCache
{
public:

    CredentialsCache();
    ~CredentialsCache();

    bool getForPath(const String& protocol, const String& host,
                    short port, const String& path,
                    CredentialEntry& entry);

    bool getForRealm(const String& protocol, const String& host,
                     short port, const String& realm,
                     CredentialEntry& entry);

    void add(const CredentialEntry& entry);

    void remove(const String& protocol, const String& host, short port,
                const String& path, const String& realm);

private:

    HashMap<String, Vector<CredentialEntry>* > m_cache;
};

}

#endif /* CREDENTIALSCACHE_H */
