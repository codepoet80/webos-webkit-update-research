/*
 *  Copyright (C) 2008-2010 Palm, Inc.  All rights reserved.
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
 *
 *  ------------------------------------------------------------------------------
 *
 * NTLM Support is derived from libCURL implementation. Licensing for
 * CURL follows:
 *
 * Copyright (C) 1998 - 2007, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at http://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 */

#include <config.h>
#include "AuthenticationHandler.h"

#include <ResourceHandle.h>
#include <ResourceHandleInternal.h>
#include <ResourceHandleClient.h>
#include <CString.h>
#include <Base64.h>

#include <curl/curl.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>

#include <stdlib.h>
#include <strings.h>
#include <wtf/ASCIICType.h>

#include "CurlHandle.h"
#include "CredentialsCache.h"
#include "webkitpalmsettings.h"

namespace WebCore
{

AuthenticationHandler::AuthenticationHandler(CredentialsCache* cache, CurlHandle* job)
    : m_cache(cache)
    , m_chandle(job)
    , m_cancelled(false)
    , m_realmBasedAuthHeaderAdded(false)
    , m_pathBasedAuthHeaderAdded(false)
    , m_authHeaderAddded(false)
    , m_ntlmState(NtlmStateNone)
{

}

AuthenticationHandler::~AuthenticationHandler()
{

}

void AuthenticationHandler::reset()
{
    m_cancelled = false;
    m_realmBasedAuthHeaderAdded = false;
    m_pathBasedAuthHeaderAdded  = false;
}

void AuthenticationHandler::addCredentials(const KURL& url, const String& username, const String& password)
{
	m_user = username;
	m_password = password;

    removeAuthHeader();

    CredentialEntry entry;
    if (m_cache->getForPath(url.protocol(), url.host(), url.port(),
                            url.path(), entry)) {
        
        switch (entry.scheme) {
        case (ProtectionSpaceAuthenticationSchemeHTTPBasic): {
            addAuthHeader(String(genBasicResponse(url, entry)));
            m_pathBasedAuthHeaderAdded  = true;
            m_realmBasedAuthHeaderAdded = false;
            break;
        }
        case (ProtectionSpaceAuthenticationSchemeHTTPDigest): {
            addAuthHeader(String(genDigestResponse(url, entry)));
            m_pathBasedAuthHeaderAdded  = true;
            m_realmBasedAuthHeaderAdded = false;
            break;
        }
        default:
            break;
        }
    }
}

bool AuthenticationHandler::handleChallenge()
{
    ResourceHandleInternal* d = m_chandle->m_job->getInternal();

    CString authChallenge = d->m_response.httpHeaderField("WWW-Authenticate").latin1();
    if (authChallenge.isNull()) {
        m_cancelled = true;
        return false;
    }
    
    const char* url;
    curl_easy_getinfo(m_chandle->m_handle, CURLINFO_EFFECTIVE_URL, &url);
    KURL kurl(ParsedURLString,url);

    const char* ptr = authChallenge.data();

    // skip over whitespace
    while (*ptr && WTF::isASCIISpace(*ptr))
        ptr++;

    if (*ptr == 0) {
        m_cancelled = true;
        return false;
    }

    bool success = false;

    if (strncasecmp("Basic", ptr, 5) == 0) {
        ptr += 5;
        success = handleBasicChallenge(kurl, ptr);
    }
    else if (strncasecmp("Digest", ptr, 6) == 0) {
        ptr += 6;
        success = handleDigestChallenge(kurl, ptr);
    }
    else if (strncasecmp("NTLM", ptr, 4) == 0) {
        ptr += 4;
        success = handleNtlmChallenge(kurl, ptr);
    }
    else {
        fprintf(stderr, "Unsupported Authentication Scheme: %s\n", ptr);
        success = false;
    }

    m_cancelled = !success;
    return success;
}

void AuthenticationHandler::getUrlDirectory(const KURL& url, String& dir)
{
	// RFC 2617 Section 2: Credential is valid for all paths at or deeper than
	// the depth of the last symbolic element in the path field of the Request-URI
	dir = url.path();
	if (dir != "/") {
		int pos = dir.reverseFind('/');
		if (pos == 0)
			dir = "/";
		else
			dir = dir.left(pos + 1);
	}
}

bool AuthenticationHandler::handleBasicChallenge(const KURL& url, const char* str)
{
    ResourceHandleInternal* d = m_chandle->m_job->getInternal();

    CString realm;

    while (true) {

        // skip over whitespace
        while (*str && WTF::isASCIISpace(*str))
            str++;

        if (*str == 0)
            return false;

        if (strncasecmp("realm=\"", str, 7) != 0) {
            // unknown auth-param. skip it
            while (*str && !WTF::isASCIISpace(*str))
                str++;

            if (*str == 0)
                return false;

            continue;
        }

        // we found the realm token.
        str += 7;

        // find the end quote
        const char* endPtr = str;
        while (*endPtr && *endPtr != '\"')
            endPtr++;

        if (*endPtr == 0)
            return false;

        realm = CString(str, endPtr - str);
        break;
    }

    bool retrying = false;

    // Did we already try and fail?
    if (m_realmBasedAuthHeaderAdded) {
        m_cache->remove(url.protocol(), url.host(), url.port(),
                        String(), realm.data());
        retrying = true;
    }
    else if (m_pathBasedAuthHeaderAdded) {
        m_cache->remove(url.protocol(), url.host(), url.port(),
                        url.path(), String());
        retrying = true;
    }

    // Do we have cached credentials for this?
    CredentialEntry entry;
    if (m_cache->getForRealm(url.protocol(), url.host(), url.port(),
                             realm.data(), entry)) {
        removeAuthHeader();
        addAuthHeader(genBasicResponse(url, entry));
        m_pathBasedAuthHeaderAdded  = false;
        m_realmBasedAuthHeaderAdded = true;
        return true;
    }

	if (!m_user.isEmpty() && !retrying) {
		// Use the username/password we already have.
	}
	else if (d->client()) {
		ProtectionSpace ps(url.host(), url.port(),
						   url.protocol() == "https" ?
						   ProtectionSpaceServerHTTPS : ProtectionSpaceServerHTTP,
						   realm.data(), ProtectionSpaceAuthenticationSchemeHTTPBasic);
		Credential cr("", "", CredentialPersistenceNone);

		ResourceError err;
		d->client()->didReceiveAuthenticationChallenge(m_chandle->m_job, 
					AuthenticationChallenge(ps, cr, 0, d->m_response, err, m_chandle->m_job)
					);
	}
	else {
		m_cancelled = true;
	}

	// We could have been cancelled by the user
	if (m_cancelled)
		return false;

	String directory;
	getUrlDirectory(url, directory);

	// Cache the credentials
	entry.scheme   = ProtectionSpaceAuthenticationSchemeHTTPBasic;
	entry.protocol = url.protocol();
	entry.host     = url.host();
	entry.port     = url.port();
	entry.path     = directory;
	entry.realm    = realm.data();
	entry.user     = m_user;
	entry.password = m_password;

	// use cache only for BrowserServer to prevent "re-use" of credentials
	// between different apps
	if (PalmBrowserSettings()->runningInBrowserServer)
	    m_cache->add(entry);

	// add the authorization header to CURL
	removeAuthHeader();
	addAuthHeader(genBasicResponse(url, entry));
	m_pathBasedAuthHeaderAdded  = true;
	m_realmBasedAuthHeaderAdded = false;
        
    return true;
}

bool AuthenticationHandler::handleDigestChallenge(const KURL& url, const char* str)
{
    ResourceHandleInternal* d = m_chandle->m_job->getInternal();

    CString realm, domain, nonce, opaque, stale, algorithm, qop;

    while (true) {

        // skip over whitespace and comma
        while (*str && (WTF::isASCIISpace(*str) || *str == ','))
            str++;

        // end of string?
        if (*str == 0)
            break;

        // quoted
        if (strncasecmp("realm=\"", str, 7) == 0) {

            str += 7;

            // find the end quote
            const char* endPtr = str;
            while (*endPtr && *endPtr != '\"')
                endPtr++;

            realm = CString(str, endPtr - str);
            str = *endPtr ? endPtr + 1 : endPtr;
        }
        // quoted
        else if (strncasecmp("domain=\"", str, 8) == 0) {

            str += 8;

            // find the end quote
            const char* endPtr = str;
            while (*endPtr && *endPtr != '\"')
                endPtr++;

            domain = CString(str, endPtr - str);
            str = *endPtr ? endPtr + 1 : endPtr;
        }
        // quoted
        else if (strncasecmp("nonce=\"", str, 7) == 0) {

            str += 7;

            // find the end quote
            const char* endPtr = str;
            while (*endPtr && *endPtr != '\"')
                endPtr++;

            nonce = CString(str, endPtr - str);
            str = *endPtr ? endPtr + 1 : endPtr;
        }
        // quoted
        else if (strncasecmp("opaque=\"", str, 8) == 0) {

            str += 8;

            // find the end quote
            const char* endPtr = str;
            while (*endPtr && *endPtr != '\"')
                endPtr++;

            opaque = CString(str, endPtr - str);
            str = *endPtr ? endPtr + 1 : endPtr;
        }
        // not quoted
        else if (strncasecmp("stale=", str, 6) == 0) {

            str += 6;

            // find the end of this string
            const char* endPtr = str;
            while (*endPtr && !WTF::isASCIISpace(*endPtr) && *endPtr != ',')
                endPtr++;

            stale = CString(str, endPtr - str);
            str = *endPtr ? endPtr + 1 : endPtr;
        }
        // not quoted
        else if (strncasecmp("algorithm=", str, 10) == 0) {

            str += 10;

            // find the end of this string
            const char* endPtr = str;
            while (*endPtr && !WTF::isASCIISpace(*endPtr) && *endPtr != ',')
                endPtr++;

            algorithm = CString(str, endPtr - str);
            str = *endPtr ? endPtr + 1 : endPtr;

            if (algorithm != "MD5" && algorithm != "MD5-sess")
                // unrecognized algorithm
                algorithm = "MD5";
        }
        // quoted
        else if (strncasecmp("qop=\"", str, 5) == 0) {

            str += 5;

            // find the end quote
            const char* endPtr = str;
            while (*endPtr && *endPtr != '\"')
                endPtr++;

            qop = CString(str, endPtr - str);
            str = *endPtr ? endPtr + 1 : endPtr;

            if (qop != "auth" && qop != "auth-int")
                // unrecognized qop
                qop = CString();
        }
        else {
            // unknown token. Skip it
            while (*str && !WTF::isASCIISpace(*str))
                str++;

            if (*str == 0)
                break;
        }
    }

    // realm is not optional
    if (realm.isNull())
        return false;

    // neither is nonce
    if (nonce.isNull())
        return false;

    // Default values
    if (algorithm.isNull())
        algorithm = "MD5";

    if (qop.isNull())
        qop = "";

    // FIXME: Use stale param

    // FIXME: Use domain param

    // Did we already try and fail?
    if (m_realmBasedAuthHeaderAdded) {
        m_cache->remove(url.protocol(), url.host(), url.port(),
                        String(), realm.data());
    }
    else if (m_pathBasedAuthHeaderAdded) {
        m_cache->remove(url.protocol(), url.host(), url.port(),
                        url.path(), String());
    }

    // Do we have cached credentials for this?
    CredentialEntry entry;
    if (m_cache->getForRealm(url.protocol(), url.host(), url.port(),
                             realm.data(), entry)) {
        removeAuthHeader();
        addAuthHeader(genDigestResponse(url, entry));
        m_pathBasedAuthHeaderAdded  = false;
        m_realmBasedAuthHeaderAdded = true;
        return true;
    }

	if (!m_user.isEmpty()) {
		// Use the username/password we already have.
	}
	else if (d->client()) {

		// Prompt the user for user/pass
        ProtectionSpace ps(url.host(), url.port(),
                           url.protocol() == "https" ?
                           ProtectionSpaceServerHTTPS : ProtectionSpaceServerHTTP,
                           realm.data(), ProtectionSpaceAuthenticationSchemeHTTPDigest);
        Credential cr("", "", CredentialPersistenceNone);

		ResourceError err;
        d->client()->didReceiveAuthenticationChallenge(m_chandle->m_job, 
					AuthenticationChallenge(ps, cr, 0, d->m_response, err, m_chandle->m_job));
	}
	else {
		// No initial user/pass and can't ask user so quit.
		m_cancelled = true;
	}

	if (m_cancelled)
		return false;

	// FIXME: True for digest?
	String directory;
	getUrlDirectory(url, directory);

	// Cache the credentials        
	entry.scheme   = ProtectionSpaceAuthenticationSchemeHTTPDigest;
	entry.protocol = url.protocol();
	entry.host     = url.host();
	entry.port     = url.port();
	entry.path     = directory;
	entry.realm    = realm.data();
	entry.user     = m_user;
	entry.password = m_password;
	entry.digestNonce     = nonce.data();
	entry.digestAlgorithm = algorithm.data();
	entry.digestQop       = qop.data();
	entry.digestOpaque    = opaque.data();

	// use cache only for BrowserServer to prevent "re-use" of credentials
	// between different apps
	if (PalmBrowserSettings()->runningInBrowserServer)
	    m_cache->add(entry);

	// Add the authorization header
	removeAuthHeader();
	addAuthHeader(genDigestResponse(url, entry));
	m_pathBasedAuthHeaderAdded  = true;
	m_realmBasedAuthHeaderAdded = false;        

    return true;
}

// "NTLMSSP" signature is always in ASCII regardless of the platform
#define NTLMSSP_SIGNATURE "\x4e\x54\x4c\x4d\x53\x53\x50"

bool AuthenticationHandler::handleNtlmChallenge(const KURL& url, const char* str)
{
    ResourceHandleInternal* d = m_chandle->m_job->getInternal();
    CString ntlmString;

    while (true) {
        
        // skip over whitespace
        while (*str && WTF::isASCIISpace(*str))
            str++;

        if (*str == 0)
            break;

        // find the end of this string
        const char* endPtr = str;
        while (*endPtr && !WTF::isASCIISpace(*endPtr))
            endPtr++;

        ntlmString = CString(str, endPtr - str);
        break;
    }

    if (ntlmString.isNull()) {
        // we got a Type 1 message
        m_ntlmState = NtlmStateType1;

        CredentialEntry entry;
        entry.scheme = ProtectionSpaceAuthenticationSchemeNTLM;
        
        removeAuthHeader();
        addAuthHeader(genNtlmResponse(url, entry));
        m_pathBasedAuthHeaderAdded  = false;
        m_realmBasedAuthHeaderAdded = false;
        return true;
    }
    else {
        // we got a Type 2 message
        m_ntlmState = NtlmStateType3;

        // We use the host as the realm for NTLM
        String realm = url.host();         

        // Parse the NTLM Type 2 message
        static const char type2Marker[] = { 0x02, 0x00, 0x00, 0x00 };

        Vector<char> ntlmStringDecoded;
        base64Decode(ntlmString.data(), ntlmString.length(), ntlmStringDecoded);
        
        if (ntlmStringDecoded.size() < 32 ||
            memcmp(ntlmStringDecoded.data(), NTLMSSP_SIGNATURE, 8) != 0 ||
            memcmp(ntlmStringDecoded.data() + 8, type2Marker, sizeof(type2Marker)) != 0) {
            // Bad Type2 message
            printf("NTLM Type 2 message corrupt\n");
            return false;
        }

        unsigned int  flags;
        unsigned char nonce[8];

        memcpy(&flags, ntlmStringDecoded.data() + 20, 4);
        memcpy(nonce, ntlmStringDecoded.data() + 24, 8);

        // Did we already try and fail?
        if (m_realmBasedAuthHeaderAdded) {
            m_cache->remove(url.protocol(), url.host(), url.port(),
                            String(), realm);
        }
        else if (m_pathBasedAuthHeaderAdded) {
            m_cache->remove(url.protocol(), url.host(), url.port(),
                            url.path(), String());
        }

        // Do we have cached credentials for this?
        CredentialEntry entry;
        if (m_cache->getForRealm(url.protocol(), url.host(), url.port(),
                                 realm.latin1().data(), entry)) {

            entry.ntlmFlags = flags;
            memcpy(entry.ntlmNonce, nonce, 8);
            
            removeAuthHeader();
            addAuthHeader(genNtlmResponse(url, entry));
            m_pathBasedAuthHeaderAdded  = false;
            m_realmBasedAuthHeaderAdded = true;
            return true;
        }

		if (!m_user.isEmpty()) {
			// Use the username/password we already have.
		}
		else if (d->client()) {

			ProtectionSpace ps(url.host(), url.port(),
							   url.protocol() == "https" ?
							   ProtectionSpaceServerHTTPS : ProtectionSpaceServerHTTP,
							   realm, ProtectionSpaceAuthenticationSchemeNTLM);
			Credential cr("", "", CredentialPersistenceNone);


			ResourceError err;
			d->client()->didReceiveAuthenticationChallenge(m_chandle->m_job, 
					AuthenticationChallenge(ps, cr, 0, d->m_response, err, m_chandle->m_job));
		}
		else {
			// Don't have an initial user/pass in URL and can't ask the user.
			m_cancelled = true;
		}

		// We could have been cancelled by the user
		if (m_cancelled)
			return false;

		// FIXME: True for NTLM?
		String directory;
		getUrlDirectory(url, directory);

		// Cache the credentials        
		entry.scheme   = ProtectionSpaceAuthenticationSchemeNTLM;
		entry.protocol = url.protocol();
		entry.host     = url.host();
		entry.port     = url.port();
		entry.path     = directory;
		entry.realm    = realm;
		entry.user     = m_user;
		entry.password = m_password;
		entry.ntlmFlags = flags;
		memcpy(entry.ntlmNonce, nonce, 8);

		// use cache only for BrowserServer to prevent "re-use" of credentials
		// between different apps
		if (PalmBrowserSettings()->runningInBrowserServer)
		      m_cache->add(entry);

		// Add any existing authorization header
		removeAuthHeader();
		addAuthHeader(genNtlmResponse(url, entry));
		m_pathBasedAuthHeaderAdded  = true;
		m_realmBasedAuthHeaderAdded = false;
		
		return true;
    }

    return false;
}

/**
 * Generate the Basic Authorization response.
 */
String AuthenticationHandler::genBasicResponse(const KURL& url, const CredentialEntry& entry)
{
    String credentials = entry.user + ":" + entry.password;
    
    CString credentialsLatin1 = credentials.latin1();
    Vector<char>    credentialsDecoded;
    credentialsDecoded.append(credentialsLatin1.data(), credentialsLatin1.length());

    Vector<char>    credentialsEncoded(credentialsDecoded.size());
    base64Encode(credentialsDecoded, credentialsEncoded);
    credentialsEncoded.append('\0');
    String response("Authorization: Basic ");
    response += credentialsEncoded.data();

    return response;
}

// Following functions are from RFC 2617 spec

#define HASHLEN 16
typedef char HASH[HASHLEN];
#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN+1];

static void DigestCvtHex(HASH Bin, HASHHEX Hex)
{
    unsigned short i;
    unsigned char j;

    for (i = 0; i < HASHLEN; i++) {
        j = (Bin[i] >> 4) & 0xf;
        if (j <= 9)
            Hex[i*2] = (j + '0');
        else
            Hex[i*2] = (j + 'a' - 10);
        j = Bin[i] & 0xf;
        if (j <= 9)
            Hex[i*2+1] = (j + '0');
        else
            Hex[i*2+1] = (j + 'a' - 10);
    };
    Hex[HASHHEXLEN] = '\0';
};

// calculate H(A1) as per spec
static void DigestCalcHA1(const char * pszAlg,
                          const char * pszUserName,
                          const char * pszRealm,
                          const char * pszPassword,
                          const char * pszNonce,
                          const char * pszCNonce,
                          HASHHEX SessionKey)
{
    MD5_CTX Md5Ctx;
    HASH HA1;

    MD5_Init(&Md5Ctx);
    MD5_Update(&Md5Ctx, pszUserName, strlen(pszUserName));
    MD5_Update(&Md5Ctx, ":", 1);
    MD5_Update(&Md5Ctx, pszRealm, strlen(pszRealm));
    MD5_Update(&Md5Ctx, ":", 1);
    MD5_Update(&Md5Ctx, pszPassword, strlen(pszPassword));
    MD5_Final((unsigned char*) HA1, &Md5Ctx);
    if (strcasecmp(pszAlg, "md5-sess") == 0) {
        MD5_Init(&Md5Ctx);
        MD5_Update(&Md5Ctx, HA1, HASHLEN);
        MD5_Update(&Md5Ctx, ":", 1);
        MD5_Update(&Md5Ctx, pszNonce, strlen(pszNonce));
        MD5_Update(&Md5Ctx, ":", 1);
        MD5_Update(&Md5Ctx, pszCNonce, strlen(pszCNonce));
        MD5_Final((unsigned char*) HA1, &Md5Ctx);
    };

    DigestCvtHex(HA1, SessionKey);
}

// calculate request-digest/response-digest as per HTTP Digest spec
static void DigestCalcResponse(HASHHEX HA1,                 // H(A1)
                               const char * pszNonce,       // nonce from server
                               const char * pszNonceCount,  // 8 hex digits
                               const char * pszCNonce,      // client nonce
                               const char * pszQop,         // qop-value: "", "auth", "auth-int"
                               const char * pszMethod,      // method from the request
                               const char * pszDigestUri,   // requested URL
                               HASHHEX HEntity,             // H(entity body) if qop="auth-int"
                               HASHHEX Response)            // request-digest or response-digest
{
    MD5_CTX Md5Ctx;
    HASH HA2;
    HASH RespHash;
    HASHHEX HA2Hex;

    // calculate H(A2)
    MD5_Init(&Md5Ctx);
    MD5_Update(&Md5Ctx, pszMethod, strlen(pszMethod));
    MD5_Update(&Md5Ctx, ":", 1);
    MD5_Update(&Md5Ctx, pszDigestUri, strlen(pszDigestUri));
    if (strcasecmp(pszQop, "auth-int") == 0) {
        MD5_Update(&Md5Ctx, ":", 1);
        MD5_Update(&Md5Ctx, HEntity, HASHHEXLEN);
    };
    MD5_Final((unsigned char*)HA2, &Md5Ctx);
    DigestCvtHex(HA2, HA2Hex);

    // calculate response
    MD5_Init(&Md5Ctx);
    MD5_Update(&Md5Ctx, HA1, HASHHEXLEN);
    MD5_Update(&Md5Ctx, ":", 1);
    MD5_Update(&Md5Ctx, pszNonce, strlen(pszNonce));
    MD5_Update(&Md5Ctx, ":", 1);
    if (*pszQop) {
        MD5_Update(&Md5Ctx, pszNonceCount, strlen(pszNonceCount));
        MD5_Update(&Md5Ctx, ":", 1);
        MD5_Update(&Md5Ctx, pszCNonce, strlen(pszCNonce));
        MD5_Update(&Md5Ctx, ":", 1);
        MD5_Update(&Md5Ctx, pszQop, strlen(pszQop));
        MD5_Update(&Md5Ctx, ":", 1);
    };
    MD5_Update(&Md5Ctx, HA2Hex, HASHHEXLEN);
    MD5_Final((unsigned char*)RespHash, &Md5Ctx);
    DigestCvtHex(RespHash, Response);
};

String AuthenticationHandler::genDigestResponse(const KURL& url, const CredentialEntry& entry)
{
    ResourceHandleInternal* d = m_chandle->m_job->getInternal();

    // Generate the Digest Authorization response
    String response("Authorization: Digest ");

    response += "username=\"";
    response += entry.user;
    response += "\", ";
    
    response += "realm=\"";
    response += entry.realm;
    response += "\", ";

    response += "uri=";
    response += url.path();
    response += ", ";

    response += "nonce=\"";
    response += entry.digestNonce;
    response += "\"";

    response += ", algorithm=";
    response += entry.digestAlgorithm;

    if (!entry.digestOpaque.isEmpty()) {
        response += ", opaque=\"";
        response += entry.digestOpaque;
        response += "\"";
    }

    CString cnonce;
    CString nonceCount;
    
    if (!entry.digestQop.isEmpty()) {
        response += ", qop=";
        response += entry.digestQop;

        char cnonceStr[64];
        sprintf(cnonceStr, "%ld", random());
        cnonce = cnonceStr;

        response += ", cnonce=\"";
        response += cnonce.data();
        response += "\"";

        // FIXME: nonce count needs to vary according to RFC 2617
        nonceCount = "00000001";
        response += ", nc=";
        response += nonceCount.data();
    }
    else {
        cnonce = "";
        nonceCount = "";
    }

    HASHHEX HA1;
    HASHHEX HA2 = "";
    HASHHEX requestDigest;

    DigestCalcHA1(entry.digestAlgorithm.latin1().data(),
                  entry.user.latin1().data(),
                  entry.realm.latin1().data(),
                  entry.password.latin1().data(),
                  entry.digestNonce.latin1().data(),
                  cnonce.data(),
                  HA1);

    DigestCalcResponse(HA1,
                       entry.digestNonce.latin1().data(),
                       nonceCount.data(),
                       cnonce.data(),
                       entry.digestQop.latin1().data(),
                       m_chandle->m_job->firstRequest().httpMethod().latin1().data(),
                       url.path().latin1().data(),
                       HA2,
                       requestDigest);
    
    response += ", response=\"";
    response += requestDigest;
    response += "\"";

    return response;
}

/* Flag bits definitions based on http://davenport.sourceforge.net/ntlm.html */

#define NTLMFLAG_NEGOTIATE_UNICODE               (1<<0)
/* Indicates that Unicode strings are supported for use in security buffer
   data. */

#define NTLMFLAG_NEGOTIATE_OEM                   (1<<1)
/* Indicates that OEM strings are supported for use in security buffer data. */

#define NTLMFLAG_REQUEST_TARGET                  (1<<2)
/* Requests that the server's authentication realm be included in the Type 2
   message. */

/* unknown (1<<3) */
#define NTLMFLAG_NEGOTIATE_SIGN                  (1<<4)
/* Specifies that authenticated communication between the client and server
   should carry a digital signature (message integrity). */

#define NTLMFLAG_NEGOTIATE_SEAL                  (1<<5)
/* Specifies that authenticated communication between the client and server
   should be encrypted (message confidentiality). */

#define NTLMFLAG_NEGOTIATE_DATAGRAM_STYLE        (1<<6)
/* Indicates that datagram authentication is being used. */

#define NTLMFLAG_NEGOTIATE_LM_KEY                (1<<7)
/* Indicates that the LAN Manager session key should be used for signing and
   sealing authenticated communications. */

#define NTLMFLAG_NEGOTIATE_NETWARE               (1<<8)
/* unknown purpose */

#define NTLMFLAG_NEGOTIATE_NTLM_KEY              (1<<9)
/* Indicates that NTLM authentication is being used. */

/* unknown (1<<10) */

#define NTLMFLAG_NEGOTIATE_ANONYMOUS             (1<<11)
/* Sent by the client in the Type 3 message to indicate that an anonymous
   context has been established. This also affects the response fields. */

#define NTLMFLAG_NEGOTIATE_DOMAIN_SUPPLIED       (1<<12)
/* Sent by the client in the Type 1 message to indicate that a desired
   authentication realm is included in the message. */

#define NTLMFLAG_NEGOTIATE_WORKSTATION_SUPPLIED  (1<<13)
/* Sent by the client in the Type 1 message to indicate that the client
   workstation's name is included in the message. */

#define NTLMFLAG_NEGOTIATE_LOCAL_CALL            (1<<14)
/* Sent by the server to indicate that the server and client are on the same
   machine. Implies that the client may use a pre-established local security
   context rather than responding to the challenge. */

#define NTLMFLAG_NEGOTIATE_ALWAYS_SIGN           (1<<15)
/* Indicates that authenticated communication between the client and server
   should be signed with a "dummy" signature. */

#define NTLMFLAG_TARGET_TYPE_DOMAIN              (1<<16)
/* Sent by the server in the Type 2 message to indicate that the target
   authentication realm is a domain. */

#define NTLMFLAG_TARGET_TYPE_SERVER              (1<<17)
/* Sent by the server in the Type 2 message to indicate that the target
   authentication realm is a server. */

#define NTLMFLAG_TARGET_TYPE_SHARE               (1<<18)
/* Sent by the server in the Type 2 message to indicate that the target
   authentication realm is a share. Presumably, this is for share-level
   authentication. Usage is unclear. */

#define NTLMFLAG_NEGOTIATE_NTLM2_KEY             (1<<19)
/* Indicates that the NTLM2 signing and sealing scheme should be used for
   protecting authenticated communications. */

#define NTLMFLAG_REQUEST_INIT_RESPONSE           (1<<20)
/* unknown purpose */

#define NTLMFLAG_REQUEST_ACCEPT_RESPONSE         (1<<21)
/* unknown purpose */

#define NTLMFLAG_REQUEST_NONNT_SESSION_KEY       (1<<22)
/* unknown purpose */

#define NTLMFLAG_NEGOTIATE_TARGET_INFO           (1<<23)
/* Sent by the server in the Type 2 message to indicate that it is including a
   Target Information block in the message. */

/* unknown (1<24) */
/* unknown (1<25) */
/* unknown (1<26) */
/* unknown (1<27) */
/* unknown (1<28) */

#define NTLMFLAG_NEGOTIATE_128                   (1<<29)
/* Indicates that 128-bit encryption is supported. */

#define NTLMFLAG_NEGOTIATE_KEY_EXCHANGE          (1<<30)
/* Indicates that the client will provide an encrypted master key in
   the "Session Key" field of the Type 3 message. */

#define NTLMFLAG_NEGOTIATE_56                    (1<<31)
/* Indicates that 56-bit encryption is supported. */

#define SHORTPAIR(x) ((x) & 0xff), (((x) >> 8) & 0xff)
#define LONGQUARTET(x) ((x) & 0xff), (((x) >> 8)&0xff), \
  (((x) >>16)&0xff), (((x)>>24) & 0xff)

/*
 * Turns a 56 bit key into the 64 bit, odd parity key and sets the key.  The
 * key schedule ks is also set.
 */
static void setup_des_key(unsigned char *key_56,
                          DES_key_schedule* ks)
{
    DES_cblock key;

    key[0] = key_56[0];
    key[1] = (unsigned char)(((key_56[0] << 7) & 0xFF) | (key_56[1] >> 1));
    key[2] = (unsigned char)(((key_56[1] << 6) & 0xFF) | (key_56[2] >> 2));
    key[3] = (unsigned char)(((key_56[2] << 5) & 0xFF) | (key_56[3] >> 3));
    key[4] = (unsigned char)(((key_56[3] << 4) & 0xFF) | (key_56[4] >> 4));
    key[5] = (unsigned char)(((key_56[4] << 3) & 0xFF) | (key_56[5] >> 5));
    key[6] = (unsigned char)(((key_56[5] << 2) & 0xFF) | (key_56[6] >> 6));
    key[7] = (unsigned char) ((key_56[6] << 1) & 0xFF);

    DES_set_odd_parity(&key);
    DES_set_key(&key, ks);
}

/*
 * takes a 21 byte array and treats it as 3 56-bit DES keys. The
 * 8 byte plaintext is encrypted with each key and the resulting 24
 * bytes are stored in the results array.
 */
static void lm_resp(unsigned char *keys,
                    unsigned char *plaintext,
                    unsigned char *results)
{
    DES_key_schedule ks;

    setup_des_key(keys, &ks);
    DES_ecb_encrypt((DES_cblock*) plaintext, (DES_cblock*) results,
                    &ks, DES_ENCRYPT);

    setup_des_key(keys+7, &ks);
    DES_ecb_encrypt((DES_cblock*) plaintext, (DES_cblock*) (results+8),
                    &ks, DES_ENCRYPT);

    setup_des_key(keys+14, &ks);
    DES_ecb_encrypt((DES_cblock*) plaintext, (DES_cblock*) (results+16),
                    &ks, DES_ENCRYPT);
}


/*
 * Set up lanmanager hashed password
 */
static void mk_lm_hash(const char *password, 
                       unsigned char *lmbuffer /* 21 bytes */)
{
    unsigned char pw[14];
    static const unsigned char magic[] = {
        0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25 /* i.e. KGS!@#$% */
    };
    unsigned int i;
    size_t len = strlen(password);

    if(len > 14)
        len = 14;

    for (i=0; i<len; i++)
        pw[i] = (unsigned char)WTF::toASCIIUpper(password[i]);

    for (; i<14; i++)
        pw[i] = 0;

    {
        /* Create LanManager hashed password. */

        DES_key_schedule ks;

        setup_des_key(pw, &ks);
        DES_ecb_encrypt((DES_cblock *)magic, (DES_cblock *)lmbuffer,
                        &ks, DES_ENCRYPT);

        setup_des_key(pw+7, &ks);
        DES_ecb_encrypt((DES_cblock *)magic, (DES_cblock *)(lmbuffer+8),
                        &ks, DES_ENCRYPT);

        memset(lmbuffer + 16, 0, 21 - 16);
    }
}

static void utf8_to_unicode_le(unsigned char *dest, const char *src,
                               size_t srclen)
{
    size_t i;
    for (i=0; i<srclen; i++) {
        dest[2*i]   = (unsigned char)src[i];
        dest[2*i+1] =   '\0';
    }
}

/*
 * Set up nt hashed passwords
 */
static void mk_nt_hash(const char *password,
                       unsigned char *ntbuffer /* 21 bytes */)
{
    size_t len = strlen(password);
    unsigned char *pw = (unsigned char*) malloc(len*2);

    utf8_to_unicode_le(pw, password, len);

    {
        /* Create NT hashed password. */
        MD4_CTX MD4pw;

        MD4_Init(&MD4pw);
        MD4_Update(&MD4pw, pw, 2*len);
        MD4_Final(ntbuffer, &MD4pw);

        memset(ntbuffer + 16, 0, 21 - 16);
    }

    free(pw);
}


String AuthenticationHandler::genNtlmResponse(const KURL& url, const CredentialEntry& entry)
{
    const char* domain = "";
    const char* host   = "";
    int hostLen        = 0;
    int domLen         = 0;
    int hostOff        = 0;
    int domOff         = 0;


    if (m_ntlmState == NtlmStateType1) {

        Vector<char> ntlmBuf(32); // 32 bytes should be sufficient as host & domain are empty
        Vector<char> ntlmBufEncoded;

        snprintf((char*) ntlmBuf.data(), ntlmBuf.size(),
                 NTLMSSP_SIGNATURE "%c"
                 "\x01%c%c%c" // 32-bit type = 1
                 "%c%c%c%c"   // 32-bit NTLM flag field
                 "%c%c"       // domain length
                 "%c%c"       // domain allocated space
                 "%c%c"       // domain name offset
                 "%c%c"       // 2 zeroes
                 "%c%c"       // host length
                 "%c%c"       // host allocated space
                 "%c%c"       // host name offset
                 "%c%c"       // 2 zeroes
                 "%s"         // host name
                 "%s",        // domain string
                 0,           // trailing zero
                 0,0,0,       // part of type-1 long

                 LONGQUARTET(
                     NTLMFLAG_NEGOTIATE_OEM |
                     NTLMFLAG_REQUEST_TARGET |
                     NTLMFLAG_NEGOTIATE_NTLM_KEY |
                     NTLMFLAG_NEGOTIATE_NTLM2_KEY |
                     NTLMFLAG_NEGOTIATE_ALWAYS_SIGN
                 ),
                 SHORTPAIR(domLen),
                 SHORTPAIR(domLen),
                 SHORTPAIR(domOff),
                 0,0,
                 SHORTPAIR(hostLen),
                 SHORTPAIR(hostLen),
                 SHORTPAIR(hostOff),
                 0,0,
                 host,
                 domain);

        base64Encode(ntlmBuf, ntlmBufEncoded);
        ntlmBufEncoded.resize(ntlmBufEncoded.size() + 1);
        ntlmBufEncoded.at(ntlmBufEncoded.size() - 1) = 0;
        
        String response("Authorization: NTLM ");
        response += ntlmBufEncoded.data();

        return response;
        
    }
    else if (m_ntlmState == NtlmStateType3) {

        Vector<char> ntlmBuf(1024); // FIXME: How long should this be?
        Vector<char> ntlmBufEncoded;
        
        int lmRespOff;
        unsigned char lmResp[24]; // fixed-size
        int ntRespOff;
        unsigned char ntResp[24]; // fixed-size

        size_t userOff;
        size_t userLen;

        userLen = entry.user.length();

        if (entry.ntlmFlags & NTLMFLAG_NEGOTIATE_NTLM2_KEY) {

            unsigned char ntBuffer[0x18];
            unsigned char tmp[0x18];
            unsigned char md5sum[MD5_DIGEST_LENGTH];
            unsigned char entropy[8];
            MD5_CTX       MD5pw;

            // FIXME: This is so wrong. should be using RAND_bytes,
            // but that requires prior seeding. Need to ping folks as to
            // the right way to do this
            long int r = random();
            memcpy(entropy, &r, sizeof(r));
            //RAND_bytes(entropy,8);


            // 8 bytes random data as challenge in lmResp
            memcpy(lmResp, entropy, 8);
            // Pad with zeros
            memset(lmResp+8, 0, 0x10);

            // Fill tmp with challenge(nonce?) + entropy
            memcpy(tmp, entry.ntlmNonce, 8);
            memcpy(tmp + 8, entropy, 8);

            MD5_Init(&MD5pw);
            MD5_Update(&MD5pw, tmp, 16);
            MD5_Final(md5sum, &MD5pw);

            // We shall only use the first 8 bytes of md5sum,
            // but the des code in lm_resp only encrypt the first 8 bytes
            mk_nt_hash(entry.password.utf8().data(), ntBuffer);
            lm_resp(ntBuffer, md5sum, ntResp);
        }
        else {

            unsigned char ntBuffer[0x18];
            unsigned char lmBuffer[0x18];


            mk_nt_hash(entry.password.utf8().data(), ntBuffer);
            lm_resp(ntBuffer, const_cast<unsigned char*>(entry.ntlmNonce), ntResp);

            mk_lm_hash(entry.password.utf8().data(), lmBuffer);
            lm_resp(lmBuffer, const_cast<unsigned char*>(entry.ntlmNonce), lmResp);
        }

        lmRespOff = 64; // size of the message header
        ntRespOff = lmRespOff + 0x18;
        domOff    = ntRespOff + 0x18;
        userOff   = domOff    + domLen;
        hostOff   = userOff   + userLen;

        unsigned int ntlmFlags = entry.ntlmFlags & ~NTLMFLAG_NEGOTIATE_UNICODE;

        // Create the big type-3 message binary blob
        size_t size = snprintf((char *)ntlmBuf.data(), ntlmBuf.size(),
                               NTLMSSP_SIGNATURE "%c"
                               "\x03%c%c%c" // type-3, 32 bits

                               "%c%c" // LanManager length
                               "%c%c" // LanManager allocated space
                               "%c%c" // LanManager offset
                               "%c%c" // 2 zeroes

                               "%c%c" // NT-response length
                               "%c%c" // NT-response allocated space
                               "%c%c" // NT-response offset
                               "%c%c" // 2 zeroes

                               "%c%c"  // domain length
                               "%c%c"  // domain allocated space
                               "%c%c"  // domain name offset
                               "%c%c"  // 2 zeroes

                               "%c%c"  // user length
                               "%c%c"  // user allocated space
                               "%c%c"  // user offset
                               "%c%c"  // 2 zeroes

                               "%c%c"  // host length
                               "%c%c"  // host allocated space
                               "%c%c"  // host offset
                               "%c%c"  // 2 zeroes

                               "%c%c"  // session key length (unknown purpose)
                               "%c%c"  // session key allocated space (unknown purpose)
                               "%c%c"  // session key offset (unknown purpose)
                               "%c%c"  // 2 zeroes

                               "%c%c%c%c" // flags

                               // domain string
                               // user string
                               // host string
                               // LanManager response
                               // NT response
                               ,
                               0,     // zero termination
                               0,0,0, // type-3 long, the 24 upper bits

                               SHORTPAIR(0x18),  // LanManager response length, twice
                               SHORTPAIR(0x18),
                               SHORTPAIR(lmRespOff),
                               0x0, 0x0,

                               SHORTPAIR(0x18),  // NT-response length, twice
                               SHORTPAIR(0x18),
                               SHORTPAIR(ntRespOff),
                               0x0, 0x0,

                               SHORTPAIR(domLen),
                               SHORTPAIR(domLen),
                               SHORTPAIR(domOff),
                               0x0, 0x0,

                               SHORTPAIR(userLen),
                               SHORTPAIR(userLen),
                               SHORTPAIR(userOff),
                               0x0, 0x0,

                               SHORTPAIR(hostLen),
                               SHORTPAIR(hostLen),
                               SHORTPAIR(hostOff),
                               0x0, 0x0,

                               0x0, 0x0,
                               0x0, 0x0,
                               0x0, 0x0,
                               0x0, 0x0,

                               LONGQUARTET(ntlmFlags));


        // Append the Binary hashes
        if (size < (ntlmBuf.size() - 0x18)) {
            ASSERT(size == (size_t) lmRespOff);
            memcpy(ntlmBuf.data() + size, lmResp, 0x18);
            size += 0x18;
        }

        if (size < (ntlmBuf.size() - 0x18)) {
            ASSERT(size == (size_t)ntRespOff);
            memcpy(ntlmBuf.data() + size, ntResp, 0x18);
            size += 0x18;
        }

        memcpy(ntlmBuf.data() + size, domain, domLen);
        size += domLen;

        memcpy(ntlmBuf.data() + size, entry.user.latin1().data(), userLen);
        size += userLen;

        memcpy(ntlmBuf.data() + size, host, hostLen);
        size += hostLen;

        ntlmBuf.resize(size);
        base64Encode(ntlmBuf, ntlmBufEncoded);
        ntlmBufEncoded.resize(ntlmBufEncoded.size() + 1);
        ntlmBufEncoded.at(ntlmBufEncoded.size() - 1) = 0;
        
        String response("Authorization: NTLM ");
        response += ntlmBufEncoded.data();

        return response;
    }
    
    return String("NA NA NA");    
}

void AuthenticationHandler::receivedUserCredentials(const AuthenticationChallenge& challenge,
                                                    const Credential& credential)
{
    m_cancelled = false;
    m_user      = credential.user();
    m_password  = credential.password();
}

void AuthenticationHandler::receivedUserCancellation(const AuthenticationChallenge& challenge)
{
    m_cancelled = true;
}

void AuthenticationHandler::removeAuthHeader()
{
    if (!m_authHeaderAddded)
        return;
    
    // We add the authorization string to the curl custom headers. If there is
    // an existing Authorization header, we need to remove it
    if (m_chandle->m_customHeaders) {

        struct curl_slist* head = m_chandle->m_customHeaders;
        struct curl_slist* prev = 0;
        struct curl_slist* next = 0;
        while (head) {
            next = head->next;
            if (strncasecmp("Authorization", head->data, 13) == 0) {
                curl_free(head->data);
                curl_free(head);

                if (prev)
                    prev->next = next;
                else
                    // This was the head
                    m_chandle->m_customHeaders = next;
                break;
            }

            prev = head;
            head = next;
        }
    }

    m_realmBasedAuthHeaderAdded = false;
    m_pathBasedAuthHeaderAdded  = false;
    m_authHeaderAddded = false;
}

void AuthenticationHandler::addAuthHeader(const String& authHeader)
{
    if (authHeader.isEmpty())
        return;

    ResourceHandleInternal* d = m_chandle->m_job->getInternal();

    m_chandle->m_customHeaders = curl_slist_append(m_chandle->m_customHeaders, authHeader.latin1().data());
    curl_easy_setopt(m_chandle->m_handle, CURLOPT_HTTPHEADER, m_chandle->m_customHeaders);

    m_authHeaderAddded = true;
}

}
