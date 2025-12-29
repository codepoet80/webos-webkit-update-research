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

#include "config.h"
#include "CredentialsCache.h"

namespace WebCore
{

CredentialEntry::CredentialEntry()
{
    
}

CredentialEntry::CredentialEntry(const CredentialEntry& e)
{
    scheme           = e.scheme;
    protocol         = e.protocol;
    host             = e.host;
    port             = e.port;
    path             = e.path;
    realm            = e.realm;
    user             = e.user;
    password         = e.password;
    digestNonce      = e.digestNonce;
    digestQop        = e.digestQop;
    digestAlgorithm  = e.digestAlgorithm;
    digestOpaque     = e.digestOpaque;
    ntlmFlags        = e.ntlmFlags;

    memcpy(ntlmNonce, e.ntlmNonce, 8);    
}

CredentialEntry& CredentialEntry::operator=(const CredentialEntry& e)
{
    if (this != &e) {
        scheme           = e.scheme;
        protocol         = e.protocol;
        host             = e.host;
        port             = e.port;
        path             = e.path;
        realm            = e.realm;
        user             = e.user;
        password         = e.password;
        digestNonce      = e.digestNonce;
        digestQop        = e.digestQop;
        digestAlgorithm  = e.digestAlgorithm;
        digestOpaque     = e.digestOpaque;
        ntlmFlags        = e.ntlmFlags;

        memcpy(ntlmNonce, e.ntlmNonce, 8);    
    }

    return *this;
}



static inline String cacheKey(const String& protocol, const String& host, short port)
{
    return protocol + "\\" + host + "\\" + String::number(port);
}

CredentialsCache::CredentialsCache()
{

}

CredentialsCache::~CredentialsCache()
{
    HashMap<String, Vector<CredentialEntry>*>::iterator it = m_cache.begin();
    for (; it != m_cache.end(); ++it) {
        Vector<CredentialEntry>* credentialsList = (*it).second;
        delete credentialsList;
    }

    m_cache.clear();
}

bool CredentialsCache::getForPath(const String& protocol, const String& host,
                                  short port, const String& path,
                                  CredentialEntry& entry)
{
    String key = cacheKey(protocol, host, port);

    Vector<CredentialEntry>* credentialsList = m_cache.get(key);
    if (!credentialsList)
        return false;

    for (unsigned int i = 0; i < credentialsList->size(); i++) {
        const CredentialEntry& e = credentialsList->at(i);
        if (path.startsWith(e.path)) {
            entry = e;
            return true;
        }
    }

    return false;
}

bool CredentialsCache::getForRealm(const String& protocol, const String& host,
                                   short port, const String& realm,
                                   CredentialEntry& entry)
{
    String key = cacheKey(protocol, host, port);

    Vector<CredentialEntry>* credentialsList = m_cache.get(key);
    if (!credentialsList)
        return false;

    for (unsigned int i = 0; i < credentialsList->size(); i++) {
        const CredentialEntry& e = credentialsList->at(i);
        if (realm == e.realm) {
            entry = e;
            return true;
        }
    }

    return false;
}

void CredentialsCache::add(const CredentialEntry& entry)
{
    String key = cacheKey(entry.protocol, entry.host, entry.port);

    Vector<CredentialEntry>* credentialsList = m_cache.get(key);
    if (!credentialsList) {
        credentialsList = new Vector<CredentialEntry>;
        m_cache.set(key, credentialsList);
    }

    bool inserted = false;

    for (unsigned int i = 0; i < credentialsList->size(); i++) {
        CredentialEntry& e = credentialsList->at(i);
        if (e.scheme == entry.scheme && e.path == entry.path && e.realm == entry.realm) {
            e = entry;
            inserted = true;
            break;
        }
    }

    if (!inserted)
        credentialsList->append(entry);
}

void CredentialsCache::remove(const String& protocol, const String& host, short port,
                              const String& path, const String& realm)
{
    String key = protocol + "\\" + host + "\\" + String::number(port);

    Vector<CredentialEntry>* credentialsList = m_cache.get(key);
    if (!credentialsList)
        return;

    if (!path.isEmpty()) {
        for (unsigned int i = 0; i < credentialsList->size(); i++) {
            const CredentialEntry& e = credentialsList->at(i);
            if (path.startsWith(e.path)) {
                credentialsList->remove(i);
                return;
            }
        }
    }
    else if (!realm.isEmpty()) {
        for (unsigned int i = 0; i < credentialsList->size(); i++) {
            const CredentialEntry& e = credentialsList->at(i);
            if (realm == e.realm) {
                credentialsList->remove(i);
                return;
            }
        }
    }
}

}
