/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2008 Palm, Inc. All rights reserved
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef CURLHANDLE_H
#define CURLHANDLE_H

#include "Timer.h"
#include "ResourceResponse.h"
#include <string>
#include <list>

#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>
#include <openssl/crypto.h>
#include <openssl/lhash.h>
#include <openssl/objects.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pkcs12.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>

#include <curl/curl.h>

namespace WebCore
{

class AuthenticationChallenge;
class AuthenticationHandler;
class Credential;
class DiskCacheEntry;
class ResourceHandle;

class CurlHandle
{
public:

    CurlHandle(ResourceHandle* job);
    ~CurlHandle();

    void startAsync();
    void startSync();
    void cancel();

    void receivedCredential(const AuthenticationChallenge&, const Credential&);
    void receivedCancellation(const AuthenticationChallenge&);
	
	static void addAppendedHTTPHeader( const char* name, const char* value );
	static void removeAppendedHTTPHeader( const char* name );

	bool isInMulti() { return m_isInMulti; }
	void setInMulti(bool isInMulti) { m_isInMulti = isInMulti; }

	void useHttpProxy(const String& host, const int port);
	void useNetworkInterface(const String& interfaceName);
    void useDNSServers(const String& dnsServers);

private:

    bool setupJob();
	bool shouldAbort() const;
    void parseDataUrl();
	void abortLoading();
    void setupPUT(struct curl_slist** headers);
    void setupPOST(struct curl_slist** headers);

    static size_t headerCallback(char* ptr, size_t size, size_t nmemb, void* data);
    static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* data);
    static size_t readCallback(void* ptr, size_t size, size_t nmemb, void* data);
    static int sslVerificationCallback(int isPreverifiedOk, X509_STORE_CTX * ctx);
	static CURLcode curlSSLVerifyHookCallback(CURL * curl, void * sslctx, void * parm);
    static bool isCertValidForUrl(X509* cert, const KURL& kurl);
    static bool getCommonNames(X509_NAME* subj, std::list<std::string>& names);
	static bool sslVerificationCNameToHostnameMatch(const std::string& cname, const std::string& hostname);
	
	static bool sslVerificationCNameToHostnameMatchStrict(const std::string& cname, const std::string& hostname);
	static bool sslVerificationCNameToHostnameMatchRelaxed(const std::string& cname, const std::string& hostname);
	
	static bool nameFragmentMatched(const std::string& masterFragment,const std::string& candidateFragment,bool exactMatchOnly);

    void   beginReceiveData();
    void   commitReceiveData();
    size_t receivedHeader(char* ptr, size_t size, size_t nmemb);
    size_t receivedData(void* ptr, size_t size, size_t nmemb);
    size_t readData(void* ptr, size_t size, size_t nmemb);
    void   finishedLoading(int httpCode);
    void   failedLoading(CURLcode errorCode, const String& errorCodeString);
    void   cancelledLoading();
    void   delayedDelete();

    void   asyncTimerCallback(Timer<CurlHandle>*);
    void   delTimerCallback(Timer<CurlHandle>*);

    ResourceHandle * getJob() { return m_job;}
    
    ResourceHandle* m_job;
    CURL* m_handle;
    char* m_url;
    char* m_cookie;
    struct curl_slist* m_customHeaders;
    bool m_cancelled;

    Vector<char> m_bufferedData;

    FILE* m_file;
    size_t m_formDataElementIndex;
    size_t m_formDataElementDataOffset;
    Vector<char> m_postBytes;
    char m_curlErrorBuffer[CURL_ERROR_SIZE];
    AuthenticationHandler* m_authHandler;
    DiskCacheEntry* m_cacheEntry;
    Timer<CurlHandle> m_asyncTimer;
    Timer<CurlHandle> m_delTimer;
	
    bool	m_sslVerificationComplete;
    bool	m_sslShouldAccept;
    
	int m_httpStatus;
	int m_startTime;

	bool m_isInMulti;
	
	String m_httpProxyHost;
	int    m_httpProxyPort;
	bool   m_httpProxyEnabled;
	
	String m_networkInterfaceName;
	bool   m_forceNetworkInterfaceEnabled;

    String m_dnsServers;
    bool   m_forceDNSServersEnabled;

    friend class CurlHandlePool;
    friend class AuthenticationHandler;
    
};

}

#endif /* CURLHANDLE_H */
