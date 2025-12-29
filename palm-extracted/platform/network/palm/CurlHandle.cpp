/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005, 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2008, 2009, 2010 Palm, Inc. All rights reserved
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

#include "config.h"
#include "CurlHandle.h"

#include "AuthenticationHandler.h"
#include "Base64.h"
#include "CookieJar.h"
#include "CurlHandlePool.h"
#include "DiskCachePalm.h"
#include "FileSystem.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"
#include "TextEncoding.h"

#include "webkitpalmsettings.h"
#include "palmwebsslinfo.h"
#include <palmerrorcodes.h>
#include <algorithm>
#include <string>
#include <map>
#include <syslog.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>


namespace WebCore
{
/**
 * The certificate revocation list file path.
 */
static const char * const k_pszCRLFileLoc = 
#if defined(WEBOS_DESKTOP)
		"/var/ssl/palm/crl/crl-bundle.crl";
#else
		"/var/ssl/crl/crl-bundle.crl";
#endif


/**
 * Parses and provides a means to access a comma-separated list of params
 * which can either be strings, or name=value pairs.
 */
class HeaderValues
{
public:

	HeaderValues(const String& str) {

		Vector<String> values;
		str.split(",", false, values);

		const static String emptyString;
		Vector<String>::iterator i = values.begin();
		for (; i != values.end(); ++i) {
			Vector<String> equals;
			i->stripWhiteSpace().split("=", equals);
			if (equals.size() == 1) {
				m_values.add(equals[0], emptyString);
			}
			else if (equals.size() == 2) {
				m_values.add(equals[0], equals[1]);
			}
		}
	}

	bool contains(const String& str) const {
		HashMap<String,String>::const_iterator i = m_values.begin();
		for (; i != m_values.end(); ++i) {
			if (equalIgnoringCase(i->first, str))
				return true;
		}
		return false;
	}

	const String& getValue(const String& name) const {
		const static String emptyString;
		HashMap<String,String>::const_iterator i = m_values.begin();
		for (; i != m_values.end(); ++i) {
			if (equalIgnoringCase(i->first, name))
				return i->second;
		}
		return emptyString;
	}

private:

	HashMap<String,String> m_values;
};


static std::map<std::string,std::string> sAppendedHttpHeaders;
	
static const unsigned int maxCookieLen = 5100;

CurlHandle::CurlHandle(ResourceHandle* job)
    : m_job(job)
    , m_handle(0)
    , m_url(0)
    , m_cookie(0)
    , m_customHeaders(0)
    , m_cancelled(false)
    , m_file(0)
    , m_formDataElementIndex(0)
    , m_formDataElementDataOffset(0)
    , m_authHandler(0)
    , m_cacheEntry(0)
    , m_asyncTimer(this, &CurlHandle::asyncTimerCallback)
    , m_delTimer(this, &CurlHandle::delTimerCallback)
	, m_httpStatus(-1)
	, m_startTime(0)
	, m_sslVerificationComplete(false)
	, m_isInMulti(false)
	, m_httpProxyEnabled(false)
	, m_httpProxyPort(0)
    , m_forceNetworkInterfaceEnabled(false)
    , m_forceDNSServersEnabled(false)
{
	CurlHandlePool::instance()->watchHandle(this);
	
    m_authHandler = new AuthenticationHandler(CurlHandlePool::instance()->credentialsCache(), this);
     	
}

CurlHandle::~CurlHandle()
{
	CurlHandlePool::instance()->unwatchHandle(this);
	
    //printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    if (m_handle) {
        CurlHandlePool::instance()->release(m_handle);
		curl_easy_cleanup(m_handle);
	}

    if (m_url)
        free(m_url);
    
    if (m_customHeaders)
        curl_slist_free_all(m_customHeaders);

    if (m_cookie)
        free(m_cookie);

    if (m_file)
        fclose(m_file);

    delete m_authHandler;

    delete m_cacheEntry;
}

void CurlHandle::startAsync()
{
    //printf("Start Async: [%s]\n", m_job->request().url().string().ascii());
	if (shouldAbort()) {
        m_asyncTimer.startOneShot(0);
        return;
	}

    KURL kurl = m_job->firstRequest().url();

    String protocol = kurl.protocol().lower();
    if (protocol == "data" || protocol == "file") {
		m_asyncTimer.startOneShot(0);
		return;
    }
    
    CurlHandlePool* pool = CurlHandlePool::instance();
    m_handle = pool->acquire(this);
    if (!setupJob()) {
        delayedDelete();
        return;
    }
    
    pool->startAsync(this);
}

void CurlHandle::startSync()
{
    //printf("Start Sync: %s\n", m_job->request().url().string().ascii());
	if (shouldAbort()) {
		abortLoading();
        delayedDelete();
        return;
	}
	
    KURL kurl = m_job->firstRequest().url();
    if (equalIgnoringCase(kurl.protocol(), "data")) {
        parseDataUrl();
        delayedDelete();
        return;
    }
    
    CurlHandlePool* pool = CurlHandlePool::instance();
    m_handle = pool->acquire(this);
    if (!setupJob()) {
        delayedDelete();
        return;
    }
    
    pool->startSync(this);

	commitReceiveData();
}

void CurlHandle::cancel()
{
    CurlHandlePool::instance()->cancel(this);
    m_asyncTimer.stop();
    m_cancelled = true;
}

/**
 * Retrieve all common names in the cert.
 */
bool CurlHandle::getCommonNames(X509_NAME* subj, std::list<std::string>& names)
{
    ASSERT(names.empty());

    int lastpos(-1);

    while(true) {
        lastpos = X509_NAME_get_index_by_NID(subj, NID_commonName, lastpos);
        if (lastpos == -1)
               break;
        
        ASN1_STRING * str = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subj, lastpos));
		if (str == NULL)
			continue;

		//create string from retrieved common name
		//TODO: use multibyte aware routines
        std::string commonNameStr(reinterpret_cast<const char*>(str->data), str->length);
        names.push_back(commonNameStr);
    }

    return true;
}

/**
 * Examine the values for a certificate and determine if it can be used for the given
 * URL.
 *
 * FIXME: This function doesn't handle IP addresses. We should take a second look at
 *        curl to make sure that we can't let it do this comparison for us.
 */
bool CurlHandle::isCertValidForUrl(X509* cert, const KURL& kurl)
{
    ASSERT(cert != NULL);

    std::string host(kurl.host().utf8().data());

	bool nameMatched(false);
    std::list<std::string> names;
    if (getCommonNames(X509_get_subject_name(cert), names)) {
        std::list<std::string>::const_iterator i;
        for (i = names.begin(); !nameMatched && i != names.end(); ++i) {
            nameMatched = CurlHandle::sslVerificationCNameToHostnameMatch(*i, host);
        }
    }

    if (!nameMatched) {
        STACK_OF(GENERAL_NAME) * altnames = static_cast<STACK*>(X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL));
        if (altnames) {
			
			std::transform(host.begin(), host.end(), host.begin(), tolower); 

            int target = GEN_DNS; /* target type, GEN_DNS or GEN_IPADD */
            int numalts = sk_GENERAL_NAME_num(altnames);
            for (int i = 0; !nameMatched && i < numalts; i++) {
                const GENERAL_NAME *check = sk_GENERAL_NAME_value(altnames, i);
                if(check->type == target) {
					unsigned char* str(NULL);
					if ( ASN1_STRING_to_UTF8(&str, check->d.ia5) > 0 ) {
						std::string altSubName(reinterpret_cast<const char*>(str));
						OPENSSL_free(str);
			
						std::transform(altSubName.begin(), altSubName.end(), altSubName.begin(), tolower); 

                        nameMatched = CurlHandle::sslVerificationCNameToHostnameMatch(altSubName, host);
					}
                }
            }
        }
    }

    return nameMatched;
}

/**
 * SSL's callback to verify a certificate.
 *
 * @return Truthy value; 0 if <b>invalid</b> non zero if the certificate is ok.
 */
int CurlHandle::sslVerificationCallback(int isPreverifiedOk, X509_STORE_CTX * ctx)
{
	const int CERT_INVALID = 0;
	const int CERT_OK = 1;

	//grab the SSL object out of the passed in context
	SSL * ssl = (SSL *)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	if (ssl == NULL) {
		g_warning("sslVerificationCallback(): couldn't retrieve SSL object from X509 Store Context");
		return CERT_INVALID;
	}

	//now get the SSL_CONTEXT
	SSL_CTX * ssl_ctx= ssl->ctx;
	if (ssl_ctx == NULL) {
		g_warning("sslVerificationCallback(): couldn't retrieve SSL Context object from SSL object");
		return CERT_INVALID;
	}
	
	//now finally the CurlHandle object which is actually (HOPEFULLY!, see curlSSLVerifyHookCallback()) inside SSL_CTX's app data
	CurlHandle * curlHandle = (CurlHandle *)SSL_CTX_get_app_data(ssl_ctx);
	if (curlHandle == NULL) {
		g_warning("sslVerificationCallback(): couldn't retrieve CurlHandle object from SSL Context object");
		return CERT_INVALID;
	}
	
	//if curl request was cancelled, just return out
	if (curlHandle->m_cancelled)
		return CERT_INVALID;
	
	ResourceHandleInternal* d = curlHandle->m_job->getInternal();
	ResourceHandleClient* client = NULL;
	if (d) 
		client = d->client();

	if (client == NULL)
		return CERT_INVALID;			//unknown error (something serious if these things are null)

	KURL kurl;		//rely on the default construct to make a valid object, in case job == NULL (in some weird error case)

	std::string currentUrl;
	ResourceHandle * job = curlHandle->getJob();
	if (job) {
		kurl = job->firstRequest().url();
		currentUrl = job->firstRequest().url().string().utf8().data();
	}
	else
		return CERT_INVALID;
	
	std::string currentHostName = kurl.host().utf8().data();
	
	
	//get the error and save it
	int sslErr = X509_STORE_CTX_get_error(ctx);
	
	//g_message("CurlHandle::sslVerificationCallback: URL = [%s] , isPreverifiedOk = %d , sslError = %d [%s]",
	//		currentUrl.c_str(), isPreverifiedOk, sslErr, X509_verify_cert_error_string(sslErr));
	
	X509_STORE_CTX_set_error(ctx,X509_V_OK);		//clear the error, or else curl will be more than happy to just abort the connection later	
	SSL_set_verify_result(ssl,X509_V_OK);
	
	if (sslErr == X509_V_ERR_UNABLE_TO_GET_CRL) {	// Cannot get a CRL for the given cert
		// The cert looks OK, but can't find a CRL for this source so we're called to make the 
		// final decision. There's an informative thread on this error value at: http://marc.info/?t=105655965200004&r=1&w=2
		return CERT_OK;
	}

        if (sslErr == X509_V_ERR_CRL_HAS_EXPIRED) {
                // Got a CRL but it has expired. To prevent the trouble seen in DFISH-8811
                // we override this error, too (this should at least cause no more severe
                // security issues than ignoring the one about not getting the CRL at all
                // and the (outdated) CRL will be checked nevertheless)
                return CERT_OK;
        }

	if (sslErr == X509_V_ERR_CERT_REVOKED) {
		//g_warning("Certificate is revoked - disallowing!");
		X509_STORE_CTX_set_error(ctx,sslErr);
		SSL_set_verify_result(ssl,sslErr);
		syslog( LOG_ERR, "CurlHandle SSL error %d", sslErr );
		return CERT_INVALID;
	}

	//if already completed verification, end here
	if (curlHandle->m_sslVerificationComplete) {
		if (curlHandle->m_sslShouldAccept) {
			//g_warning("CurlHandle::sslVerificationCallback: URL = [%s] (isPreverifiedOk = %d , sslError = %d [%s]) - already verified this connection and Accepted",
			//		currentUrl.c_str(),isPreverifiedOk,sslErr,X509_verify_cert_error_string(sslErr));

			// If the user manually accepted the certificate then don't verify the hostname
			// It could be lying anyhow. This fixes NOV-59810
			if (curl_easy_setopt(curlHandle->m_handle, CURLOPT_SSL_VERIFYPEER, 0L) != CURLE_OK ) {
				g_warning("curl set opt: CURLOPT_SSL_VERIFYPEER failed.");
			}

			return CERT_OK;
		}
		else {
			//g_warning("CurlHandle::sslVerificationCallback: URL = [%s] (isPreverifiedOk = %d , sslError = %d [%s]) - already verified this connection and Rejected",
			//		currentUrl.c_str(),isPreverifiedOk,sslErr,X509_verify_cert_error_string(sslErr));
			//restore the error
			X509_STORE_CTX_set_error(ctx,sslErr);
			SSL_set_verify_result(ssl,sslErr);
			syslog( LOG_ERR, "CurlHandle SSL error %d", sslErr );
			return CERT_INVALID;
		}
	}
	
	
	X509 * cert = ctx->cert;
	
	if (cert == NULL) {
		//g_warning("CurlHandle::sslVerificationCallback: URL = [%s] (isPreverifiedOk = %d , sslError = %d [%s]) - no certificate in the X509 store!",
		//		currentUrl.c_str(),isPreverifiedOk,sslErr,X509_verify_cert_error_string(sslErr));
		//restore the error
		X509_STORE_CTX_set_error(ctx,sslErr);
		SSL_set_verify_result(ssl,sslErr);
		syslog( LOG_ERR, "CurlHandle SSL error %d", sslErr );
		return CERT_INVALID;
	}
	
	//Process all commonName entries:
	bool nameMatched = isCertValidForUrl(cert, kurl);

	if ((isPreverifiedOk == 0) || (!nameMatched))			//additional security policy: CN mismatches to host name are not strict SSL errors
															// But implementing the LemmingPolicy w.r.t. IE, Firefox, and Safari, we're flagging CN != host as
															// a cert error
	{
        std::string commonNameStr;
        std::list<std::string> names;
        if (getCommonNames(X509_get_subject_name(cert), names)) {
            if (!names.empty()) {
                commonNameStr = names.front();
            }
        }

        if (commonNameStr.empty()) {
            commonNameStr = "_unknown_";
        }

		//std::string fname = PalmBrowserSettings()->tempSSLCertLocationPath+std::string(commonName)+std::string(".pem");
		std::string fname = PalmBrowserSettings()->tempSSLCertLocationPath + commonNameStr + ".pem";
		FILE * fp = fopen(fname.c_str(),"wb");
		if (fp == NULL) {
			//restore the error
			X509_STORE_CTX_set_error(ctx,sslErr);
			SSL_set_verify_result(ssl,sslErr);
			syslog( LOG_ERR, "CurlHandle SSL error %d", sslErr );
			return CERT_INVALID;
		}
		
		PEM_write_X509(fp,cert);
//		X509_print_fp(stdout,cert);
		fclose(fp);

		/*
		 * sslErr is an OpenSSL/X509 lib specific code defined in x509_vfy.h . We'll want to localize strings for each error code in clients
		 */
		Palm::SSLValidationInfo sslInfo(fname,
										std::string("Certificate could not be automatically verified"),
										commonNameStr,
										currentHostName,
										std::string(""),		//issuer name goes here...but it isn't being used right now
										sslErr);
		client->didReceiveSSLCertValidationFail(sslInfo);

		curlHandle->m_sslVerificationComplete = true;
		
		if (sslInfo.getAcceptDecision() == Palm::SSLValidationInfo::AcceptDecision_Reject) {
			curlHandle->m_sslShouldAccept = false;
			//restore the error
			//g_warning("CurlHandle::sslVerificationCallback: URL = [%s] (isPreverifiedOk = %d , sslError = %d [%s]) - User Rejected. Restoring sslError and exiting(0)",
			//		currentUrl.c_str(),isPreverifiedOk,sslErr,X509_verify_cert_error_string(sslErr));
			X509_STORE_CTX_set_error(ctx,sslErr);
			SSL_set_verify_result(ssl,sslErr);
			syslog( LOG_ERR, "CurlHandle SSL error %d", sslErr );
			return CERT_INVALID;
		}
		else {
			curlHandle->m_sslShouldAccept = true;
			//g_warning("CurlHandle::sslVerificationCallback: URL = [%s] (isPreverifiedOk = %d , sslError = %d [%s]) - User Accepted(%d). Exiting(1)",
			//		currentUrl.c_str(),isPreverifiedOk,sslErr,X509_verify_cert_error_string(sslErr),sslInfo.getAcceptDecision());

			// If the user manually accepted the certificate then don't verify the hostname
			// It could be lying anyhow. This fixes NOV-59810
			if (curl_easy_setopt(curlHandle->m_handle, CURLOPT_SSL_VERIFYPEER, 0L) != CURLE_OK ) {
				g_warning("curl set opt: CURLOPT_SSL_VERIFYPEER failed.");
			}
		}
	}
	
	return CERT_OK;
}

/**
 * 
 * This gets called just before CURL tries to negotiate a SSL session. It allows me time to set up the callback
 * that will intercept and override verification behavior, and let me act on it
 * 
 * This would be a lot easier if CURL had separate "verify ok", "verify failed" callbacks, instead of just this one
 * 
 */
//TODO: error checks on all this pointer casting and passing around
CURLcode CurlHandle::curlSSLVerifyHookCallback(CURL * curl, void * sslctx, void * parm)
{
	//the parm points to a CurlHandle object...remember, this fn is static
	SSL_CTX * ctx = (SSL_CTX *) sslctx ;
	//SSL_CTX_set_quiet_shutdown(ctx,1);		//this was copied from reference code and apparently violates the TLS standard
	//SSL_CTX_set_cipher_list(ctx,"AES256-SHA:AES128-SHA:RC4-MD5");
	//SSL_CTX_set_cipher_list(ctx,"DHE:RSA:AES256:SHA");
	SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
	//TODO: FIXME: EXTREMELY DANGEROUS!!!! I really don't know what Curl decided to cram into app data (which I think is index=0)
	//		but there's no other way to do this apparently, except for using external maps of X509_STORE_CTX -> SSL_CTX
	SSL_CTX_set_app_data(ctx,parm);
	SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,sslVerificationCallback);
	
	return CURLE_OK ;
	//	for err:
	//return CURLE_SSL_CERTPROBLEM;
		
}

/*
 * 
 * 	Inspired by one or more of the following  (I say "inspired" because there are many points in the collective that contradict each other)
 * 
 *	http://tools.ietf.org/html/rfc2818#section-3.1
 *	http://www.ietf.org/rfc/rfc2459.txt
 *	http://nils.toedtmann.net/pub/subjectAltName.txt
 *	http://wiki.cacert.org/wiki/WildcardCertificates
 *	http://mailman.sage.org/pipermail/sage-members/2009/msg00056.html
 *
*/

bool CurlHandle::nameFragmentMatched(const std::string& masterFragment,const std::string& candidateFragment,bool exactMatchOnly)
{
	//DO NOT CHANGE ORDER OF CHECK CLAUSES

	//if both fragments are empty, it's a default match
	if ((masterFragment.length() == 0) && (candidateFragment.length() == 0))
		return true;

	//if the candidate fragment contains any wildcard characters, then No-match
	if (candidateFragment.find('*') != std::string::npos)
		return false;

	//if the master fragment is a wildcard, then it's a match
	if (masterFragment == std::string("*") && (!exactMatchOnly))
		return true;

	std::string::size_type mf_front_wild = masterFragment.find('*');
	
	//if there are no wildcards in the master fragment, then an exact match is needed
	if (mf_front_wild == std::string::npos)
		return (masterFragment == candidateFragment);

	if (exactMatchOnly)
		return false;		//at this point if it hasn't matched and an exact match is required, then No-match

	//if the master fragment contains more than 1 wildcard character (*), or 
	//the wildcard isn't the first or last, then No-match
	if (mf_front_wild != masterFragment.rfind('*'))
		return false;
	if ((mf_front_wild != 0) && (mf_front_wild != masterFragment.length()-1))
		return false;

	//Case 1: front wildcard; e.g.  *thing  CApath: none 
	if (mf_front_wild == 0) 
	{
		std::string::size_type p;
		std::string realsegment = masterFragment.substr(1);
		if ((p=candidateFragment.find(realsegment)) == std::string::npos)
			return false;
		if (p < candidateFragment.length()-realsegment.length())
			return false;
		return true;
	}
	//Case 2: back wildcard; e.g.  some* 
	if (mf_front_wild == masterFragment.length()-1)
	{
		std::string::size_type p;
		std::string realsegment = masterFragment.substr(0,masterFragment.length()-1);
		if ((p=candidateFragment.find(realsegment)) == std::string::npos)
			return false;
		if (p != 0)
			return false;
		return true;
	}

	//should not reach here
	return false;
}

bool CurlHandle::sslVerificationCNameToHostnameMatch(const std::string& cname, const std::string& hostname)
{
	if (cname.empty() || hostname.empty())
		return false;
	
	if (PalmBrowserSettings()->sslStrictNameChecking)
		return sslVerificationCNameToHostnameMatchStrict(cname,hostname);
	else
		return sslVerificationCNameToHostnameMatchRelaxed(cname,hostname);
}

//don't call directly
bool CurlHandle::sslVerificationCNameToHostnameMatchStrict(const std::string& CName, const std::string& HName) 
{
	

	std::string::size_type start_p_cname;
	std::string::size_type end_p_cname = CName.length()-1;
	std::string::size_type start_p_hname;
	std::string::size_type end_p_hname = HName.length()-1;

	std::string cname_fragment;
	std::string hname_fragment;

	bool matchedExit=false;
	bool finalSegment=false;
	bool firstSegment=true;
	while (1)
	{
		start_p_cname=CName.rfind('.',end_p_cname);
		start_p_hname=HName.rfind('.',end_p_hname);

		if ((start_p_cname == std::string::npos) && (start_p_hname == std::string::npos)) 
		{
			finalSegment=true;
			start_p_cname = 0;
			start_p_hname = 0;
		}
		else if ((start_p_cname == std::string::npos) || (start_p_hname == std::string::npos))
			return false; //mismatched length
		else if ((start_p_cname == end_p_cname) || (start_p_hname == end_p_hname))
			return false;		//malformed url
		else {
			++start_p_cname;
			++start_p_hname;
		}
		//create substrings
		cname_fragment = CName.substr(start_p_cname,end_p_cname-start_p_cname+1);
		hname_fragment = HName.substr(start_p_hname,end_p_hname-start_p_hname+1);
		
		std::transform(cname_fragment.begin(), 
				cname_fragment.end(), 
				cname_fragment.begin(), 
					tolower); 
		std::transform(hname_fragment.begin(), 
				hname_fragment.end(), 
				hname_fragment.begin(), 
					tolower); 
		
		//do a match on the fragment (e.g. "com", "palm", from palm.com...this happens in reverse order (TLD/A-level (e.g. .com) first))
		//POLICY-DECISION: Do not allow wildcards on the first segment
		if (!nameFragmentMatched(cname_fragment,hname_fragment,firstSegment))		
			return false;

		firstSegment=false;
		if (finalSegment)
			return true;

		if ((start_p_cname < 2) && (start_p_hname < 2))		//(avoid signed-unsigned comparison) 
			return true;		//both are at the last segment...==> matched
		else if ((start_p_cname < 2) || (start_p_hname < 2))
			return false;		//mismatched sizes ===> No-match
		
		//move on to the next segment
		end_p_cname = start_p_cname-2;
		end_p_hname = start_p_hname-2;
		
	}

	//should not reach here
	return false;
}

//don't call directly
bool CurlHandle::sslVerificationCNameToHostnameMatchRelaxed(const std::string& cname, const std::string& fqdn_host)
{
    std::string cname_domain(cname);

	std::transform(cname_domain.begin(), 
			cname_domain.end(), 
			cname_domain.begin(), 
			tolower); 
	
	//turn the CNAME into a 'pure' form to match on. This means discarding any leading *. sequences..
	//Examples:
	//				host.palm.com  	->   	host.palm.com
	//				*.palm.com		->		palm.com
	//				*.*.com			->		com
	
	//	only prefix wildcards are supported, so the following are illegal and will match literally
	//				host.palm.*	
	//				host.*.*
	
	//	pure wildcards are illegal:
	//				*
	//				*.*.*
	
	std::string::size_type prev_p=0;
	std::string::size_type p;
	while ((p = cname_domain.find(".",prev_p)) != std::string::npos) {
		//check the substring for any "*"
		if (cname_domain.substr(prev_p,p-prev_p).find('*') == std::string::npos)
			break; 		//no wildcard; break here
		if ((prev_p = p+1) >= cname_domain.length())
			break;	//end of string
	}
	
	if (prev_p >= cname_domain.length())
		cname_domain = std::string("");			//the string was illegal ( '.' found at length-1)
	else
		cname_domain = cname_domain.substr(prev_p);		//this is the segment that will be matched. Note that if
														//the domain was pure wildcards, or postfixed wilcards, then this
														//will cause the last segment to be "*", and matched literally
														//(which will always fail since no hostnames can be "*")
	
	std::string fqdn = std::string(fqdn_host);
	
	std::transform(fqdn.begin(), 
			fqdn.end(), 
			fqdn.begin(), 
				tolower); 
	
	//g_warning("sslVerificationCNameToHostnameMatch: matching FQDN = [%s] against CNAME = [%s]",fqdn.c_str(),cname_domain.c_str());
	
	//see if fqdn_host ends with cname_domain
	p = fqdn.find(cname_domain);
	if (p == std::string::npos) {
		//g_warning("sslVerificationCNameToHostnameMatch: matching FQDN = [%s] against CNAME = [%s] - no substring match",fqdn.c_str(),cname_domain.c_str());
		return false;		//not found at all
	}
	
	//is it anchored at the end?
	if (fqdn.length() - cname_domain.length() < p) {
		//g_warning("sslVerificationCNameToHostnameMatch: matching FQDN = [%s] against CNAME = [%s] - substring matched, but not anchored at the end (anchor at %d, should be %d)",
		//			fqdn.c_str(),cname_domain.c_str(),
		//			p,fqdn.length() - cname_domain.length());
		return false;
	}
	
	return true;
}

/**
 * Is the network interface up?
 *
 * @return true if up, false if not (or error).
 */
static bool isInterfaceUp(const char *ifName)
{
	if(!ifName)
		return false;

	struct ifreq req;
	size_t len = strlen(ifName);
	if(len >= sizeof(req.ifr_name))
		return false;

	curl_socket_t dummy = socket(AF_INET, SOCK_STREAM, 0);
	if(CURL_SOCKET_BAD == dummy)
		return false;

	memset(&req, 0, sizeof(req));
	memcpy(req.ifr_name, ifName, len+1);
	req.ifr_addr.sa_family = AF_INET;

	bool up = ioctl(dummy, SIOCGIFADDR, &req) >= 0;
	
	close(dummy);
	
	return up;
}

/**
 * Should this connection be aborted?
 */
bool CurlHandle::shouldAbort() const
{
	if (!m_forceNetworkInterfaceEnabled)
		return false;

	KURL kurl = m_job->firstRequest().url();
	if (kurl.isLocalFile())
		return false;

	// if curl cannot bind to the interface we specify then it will fallback to working as normal
	// (i.e. using any interface). We workaround this by first seeing if the interface is up. If not
	// then we fail this connection.
	CString ifname = m_networkInterfaceName.utf8();
	return !isInterfaceUp(ifname.data());
}

bool CurlHandle::setupJob()
{
	int curlSetOptRc;
	
	if (m_cancelled)
		return false;
    KURL kurl = m_job->firstRequest().url();

    // Remove any fragment part, otherwise curl will send it as part of the request.
	kurl.removeFragmentIdentifier();

    ResourceHandleInternal* d = m_job->getInternal();
    String url = kurl.string();

    if (kurl.isLocalFile()) {
        String query = kurl.query();
		
		// Remove any query part sent to a local file.
        if (!query.isEmpty())
            url = url.left(url.find(query));
        // Determine the MIME type based on the path.
        d->m_response.setMimeType(MIMETypeRegistry::getMIMETypeForPath(String(url)));
		if( d->m_response.mimeType() == String("text/html")
			|| d->m_response.mimeType() == String("text/ascii") )
			d->m_response.setTextEncodingName( "UTF-8");
		d->m_response.setURL(kurl);

		// Set this to a dummy value or we get an uninitialized value read error
		// from valgrind
		d->m_response.setHTTPStatusCode(200);
		
        // get the file size
        long long fileSizeResult=0;
        if (getFileSize(kurl.path(), fileSizeResult))
            d->m_response.setExpectedContentLength(fileSizeResult);
		
        if (d->client()) {
            d->client()->didReceiveResponse(m_job, d->m_response);
            // We could be cancelled at this point
            if (m_cancelled)
                return false;
        }
        d->m_response.setResponseFired(true);
		
		// Note : The request might have been cancelled at this point. If
		// it was, bail immediately.
		if (m_cancelled) {
			return false;
        }
    }
	
    //also, never cache HTTPS...
    // don't cache resources that are part of HTML5 app cache
    //printf("protocol = [%s]\n",kurl.protocol().utf8().data());
	if ( PalmBrowserSettings()->diskCacheEnabled && !kurl.protocolIs("https")
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
		&& !m_job->firstRequest().isAppCacheResource() ) {
#else
		) {
#endif
		m_cacheEntry = diskCache()->create();
	}

//#ifndef NDEBUG
    if (getenv("DEBUG_CURL"))
        curl_easy_setopt(m_handle, CURLOPT_VERBOSE, 1);
//#endif
    curl_easy_setopt(m_handle, CURLOPT_PRIVATE, this);
    curl_easy_setopt(m_handle, CURLOPT_ERRORBUFFER, m_curlErrorBuffer);
    curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_handle, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(m_handle, CURLOPT_WRITEHEADER, this);
    curl_easy_setopt(m_handle, CURLOPT_AUTOREFERER, 1);
    curl_easy_setopt(m_handle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(m_handle, CURLOPT_MAXREDIRS, 10);
	
    curl_easy_setopt(m_handle, CURLOPT_DNS_CACHE_TIMEOUT, 60 * 5); // 5 minutes
    // enable gzip and deflate through Accept-Encoding:
    curl_easy_setopt(m_handle, CURLOPT_ENCODING, "gzip, deflate");
	
	// Allow for slow 1X network connections to trickle down data w/o triggering an
	// idle timeout, but if less than the low limit is received then timeout.
	curl_easy_setopt(m_handle, CURLOPT_LOW_SPEED_LIMIT, 2 /* < 2 Bps for CURLOPT_LOW_SPEED_TIME will timeout. */ );
	curl_easy_setopt(m_handle, CURLOPT_LOW_SPEED_TIME, PalmBrowserSettings()->networkTimeoutSeconds );
	
	curl_easy_setopt(m_handle, CURLOPT_NOPROGRESS, 1);

  
	// SSL library(ies) MUST BE INITIALIZED prior to this point!!!!
	// (see SSLSupport.h/cpp in LunaSysMgr and BrowserServer)
        
	if ((curlSetOptRc = curl_easy_setopt(m_handle, CURLOPT_SSL_CTX_FUNCTION, curlSSLVerifyHookCallback)) != CURLE_OK )
		g_warning("curl set opt: CURLOPT_SSL_CTX_FUNCTION failed [%d]",curlSetOptRc);

	if ((curlSetOptRc = curl_easy_setopt(m_handle, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt")) != CURLE_OK )
		g_warning("curl set opt: CURLOPT_CAINFO failed [%d]",curlSetOptRc);

	if ((curlSetOptRc = curl_easy_setopt(m_handle, CURLOPT_CAPATH, "/var/ssl/certs")) != CURLE_OK )
		g_warning("curl set opt: CURLOPT_CAPATH failed [%d]",curlSetOptRc);

	if ((curlSetOptRc = curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYPEER,1L)) != CURLE_OK )
		g_warning("curl set opt: CURLOPT_SSL_VERIFYPEER failed [%d]",curlSetOptRc);

	if ((curlSetOptRc = curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYHOST,1L)) != CURLE_OK )
		g_warning("curl set opt: CURLOPT_SSL_VERIFYHOST failed [%d]",curlSetOptRc);

#if !(defined(WEBOS_DESKTOP) && (LIBCURL_VERSION_NUM <= 0x071200))
	if (access(k_pszCRLFileLoc, R_OK) != 0) {
		g_warning("Certificate revocation list missing! %s", k_pszCRLFileLoc);
		// This is a security problem that shouldn't happen so if it does I'm going to
		// consider it a hack attempt and not run.
		exit(Palm::ERR_CURL_SSL_CRL_BADFILE);
	}
	if ((curlSetOptRc = curl_easy_setopt(m_handle, CURLOPT_CRLFILE, k_pszCRLFileLoc)) != CURLE_OK )
		g_warning("curl set opt: CURLOPT_CRLFILE failed [%d]",curlSetOptRc);
#endif	

	if ((curlSetOptRc = curl_easy_setopt(m_handle, CURLOPT_SSL_CTX_DATA, this)) != CURLE_OK )
		g_warning("curl set opt: CURLOPT_SSL_CTX_DATA failed [%d]",curlSetOptRc);
		
    // url must remain valid through the request
    ASSERT(!m_url);
    m_url = strdup(url.utf8().data());
    curl_easy_setopt(m_handle, CURLOPT_URL, m_url);

    // throw out all the cookies being managed by libcurl
    curl_easy_setopt(m_handle, CURLOPT_COOKIELIST, "ALL");

    if (m_httpProxyEnabled) {
        curl_easy_setopt(m_handle, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
        curl_easy_setopt(m_handle, CURLOPT_PROXYPORT, (long) m_httpProxyPort);
        curl_easy_setopt(m_handle, CURLOPT_PROXY, m_httpProxyHost.utf8().data());
    }

    if(m_forceNetworkInterfaceEnabled)
    {
        curl_easy_setopt(m_handle, CURLOPT_INTERFACE, m_networkInterfaceName.utf8().data());    	
    }

    if(m_forceDNSServersEnabled && !m_dnsServers.isEmpty()) {
        curl_easy_setopt(m_handle, CURLOPT_SETDNSSERVERS, m_dnsServers.utf8().data());
    }


    if(m_job->firstRequest().allowCookies()) {
        // cookie must remain valid through the request
        ASSERT(!m_cookie);
        String cookieStr = cookies( d->m_document, m_job->firstRequest().url() );
        if (!cookieStr.isEmpty()) {
            m_cookie = strdup(cookieStr.utf8().data());
            curl_easy_setopt(m_handle, CURLOPT_COOKIE, m_cookie);
        }
    }


    struct curl_slist* headers = 0;
    if (m_job->firstRequest().httpHeaderFields().size() > 0) {
        HTTPHeaderMap customHeaders = m_job->firstRequest().httpHeaderFields();
        HTTPHeaderMap::const_iterator end = customHeaders.end();
        for (HTTPHeaderMap::const_iterator it = customHeaders.begin(); it != end; ++it) {
            String key = it->first;
            String value = it->second;
            
            // Allow explicit proxy/lan specifiction per request -- this is used for XHR
            if( equalIgnoringCase(key,"X-webOS-ProxyAddr") ) 
            {
            	curl_easy_setopt(m_handle, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
            	curl_easy_setopt(m_handle, CURLOPT_PROXY, value.utf8().data());
            	//printf("%s using forced proxy-addr = %s\n", url.utf8().data(), value.utf8().data() );
            }
            else if( equalIgnoringCase(key,"X-webOS-ProxyPort") ) 
            {
            	curl_easy_setopt(m_handle, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
            	curl_easy_setopt(m_handle, CURLOPT_PROXYPORT, (long)value.toUInt() );
            	//printf("%s using forced proxy-port = %d\n", url.utf8().data(), (long)value.toUInt() );
            }
            else if( equalIgnoringCase(key,"X-webOS-NetworkInterface") ) 
            {
            	curl_easy_setopt(m_handle, CURLOPT_INTERFACE, value.utf8().data());
            	//printf("%s using forced wan interface = %s\n", url.utf8().data(), value.utf8().data() );
            }
            else 
            {
				String headerString(key);
				headerString.append(": ");
				headerString.append(value);
				CString headerLatin1 = headerString.latin1();
				headers = curl_slist_append(headers, headerLatin1.data());
            }
        }
    }
	
	for( std::map<std::string,std::string>::const_iterator it=sAppendedHttpHeaders.begin(); it != sAppendedHttpHeaders.end(); ++it ) {
		std::string headerString = it->first + std::string(": ") + it->second;
		headers = curl_slist_append(headers, headerString.c_str() );
	}
	
	curl_slist_append(headers, "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3" );

    if ("GET" == m_job->firstRequest().httpMethod())
        curl_easy_setopt(m_handle, CURLOPT_HTTPGET, TRUE);
    else if ("POST" == m_job->firstRequest().httpMethod())
        setupPOST(&headers);
    else if ("PUT" == m_job->firstRequest().httpMethod())
        setupPUT(&headers);
    else if ("HEAD" == m_job->firstRequest().httpMethod())
        curl_easy_setopt(m_handle, CURLOPT_NOBODY, TRUE);
	else if (!m_job->firstRequest().httpMethod().isEmpty()) {
		curl_easy_setopt(m_handle, CURLOPT_CUSTOMREQUEST, m_job->firstRequest().httpMethod().utf8().data());
	}

    if (headers) {
        curl_easy_setopt(m_handle, CURLOPT_HTTPHEADER, headers);
        m_customHeaders = headers;
    }

    
    // Make sure to do this after the custom headers have been set
    curl_easy_setopt(m_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    m_authHandler->addCredentials(kurl, d->m_user, d->m_pass);

	if( !d->m_user.isEmpty() ) curl_easy_setopt(m_handle, CURLOPT_USERNAME, d->m_user.utf8().data() );
	if( !d->m_pass.isEmpty() ) curl_easy_setopt(m_handle, CURLOPT_PASSWORD, d->m_pass.utf8().data() );

	// Actually, the start time is buried in curl_mutli_perform() but
	// this is a crude estimate anyway so +/- a second is not going to matter.
	m_startTime = time(NULL);

    return true;
}

void CurlHandle::setupPUT(struct curl_slist** headers)
{
    Vector<FormDataElement> elements;
	size_t numElements = 0;
    curl_off_t size = 0;
	
	if( m_job->firstRequest().httpBody() )
	{
		elements = m_job->firstRequest().httpBody()->elements();
		numElements = elements.size();
	}
	
    // Obtain the total size of the PUT
    // Calculate the maximum signed value that curl_off_t can contain
    static const long long maxCurlOffT = (1ULL << (sizeof(curl_off_t) * 8 - 1)) - 1;
    bool chunkedTransfer = false;
    for (size_t i = 0; i < numElements; i++) {
        FormDataElement element = elements[i];
        if (element.m_type == FormDataElement::encodedFile) {
            long long fileSizeResult;
            if (getFileSize(element.m_filename, fileSizeResult)) {
                if (fileSizeResult > maxCurlOffT) { // If curl_off_t is a long long type then this can never be true
                    // File size is too big for specifying it to cURL
                    chunkedTransfer = true;
                    break;
                }
                size += fileSizeResult;
            } else {
                chunkedTransfer = true;
                break;
            }
        } else
            size += elements[i].m_data.size();
    }

    curl_easy_setopt(m_handle, CURLOPT_UPLOAD, TRUE);
	curl_easy_setopt(m_handle, CURLOPT_INFILESIZE , (int)size);

    // cURL guesses that we want chunked encoding as long as we specify the header
    if (chunkedTransfer)
        *headers = curl_slist_append(*headers, "Transfer-Encoding: chunked");
    else
        curl_easy_setopt(m_handle, CURLOPT_INFILESIZE_LARGE, size);
    curl_easy_setopt(m_handle, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(m_handle, CURLOPT_READDATA, this);
}

/* Calculate the length of the POST.
   Force chunked data transfer if size of files can't be obtained.
 */
void CurlHandle::setupPOST(struct curl_slist** headers)
{
	Vector<FormDataElement> elements;
	size_t numElements = 0;

	m_postBytes.clear();

	if( m_job->firstRequest().httpBody() )
	{
		elements = m_job->firstRequest().httpBody()->elements();
		numElements = elements.size();
	
		// Do not stream for simple POST data
		if (numElements == 1) {
			m_job->firstRequest().httpBody()->flatten(m_postBytes);
			curl_easy_setopt(m_handle, CURLOPT_POST, TRUE);
			curl_easy_setopt(m_handle, CURLOPT_POSTFIELDSIZE, m_postBytes.size());
			if (m_postBytes.size() != 0) {
				curl_easy_setopt(m_handle, CURLOPT_POSTFIELDS, m_postBytes.data());
			}
			else {
				curl_easy_setopt(m_handle, CURLOPT_POSTFIELDS, NULL);
			}

			// This is simple POST data, we don't want CURL to send Expect: 100-continue
			// if the data is a bit too big (which it does by default)
			*headers = curl_slist_append(*headers, "Expect:");
			return;
		}
	}
	
    // Obtain the total size of the POST
    // Calculate the maximum signed value that curl_off_t can contain
    static const long long maxCurlOffT = (1ULL << (sizeof(curl_off_t) * 8 - 1)) - 1;
    curl_off_t size = 0;
    bool chunkedTransfer = false;
    for (size_t i = 0; i < numElements; i++) {
        FormDataElement element = elements[i];
        if (element.m_type == FormDataElement::encodedFile) {
            long long fileSizeResult;
            if (getFileSize(element.m_filename, fileSizeResult)) {
                if (fileSizeResult > maxCurlOffT) { // If curl_off_t is a long long type then this can never be true
                    // File size is too big for specifying it to cURL
                    chunkedTransfer = true;
                    break;
                }
                size += fileSizeResult;
            } else {
                chunkedTransfer = true;
                break;
            }
        } else
            size += elements[i].m_data.size();
    }

    curl_easy_setopt(m_handle, CURLOPT_POST, TRUE);

    // cURL guesses that we want chunked encoding as long as we specify the header
    if (chunkedTransfer && size > 0)
        *headers = curl_slist_append(*headers, "Transfer-Encoding: chunked");
    else
        curl_easy_setopt(m_handle, CURLOPT_POSTFIELDSIZE_LARGE, size);

    curl_easy_setopt(m_handle, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(m_handle, CURLOPT_READDATA, this);
}

void CurlHandle::parseDataUrl()
{
    String url = m_job->firstRequest().url().string();
    ASSERT(url.startsWith("data:", false));

    int index = url.find(',');
    if (index == -1) {
        return;
    }    
    
    String mediaType = url.substring(5, index - 5);
    String data = url.substring(index + 1);
    
    bool base64 = mediaType.endsWith(";base64", false);
    if (base64)
        mediaType = mediaType.left(mediaType.length() - 7);

    if (mediaType.isEmpty())
        mediaType = "text/plain;charset=US-ASCII";

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);
    
    ResourceResponse response;
    response.setMimeType(mimeType);
    ResourceHandleInternal* d = m_job->getInternal();
    
    if (base64) {
        data = decodeURLEscapeSequences(data);
        response.setTextEncodingName(charset);
        d->client()->didReceiveResponse(m_job, response);

        // WebCore's decoder fails on Acid3 test 97 (whitespace).
        Vector<char> out;
        
        //if (base64Decode(data.latin1().data(), data.latin1().length(), out) && out.size() > 0)
        //    d->client()->didReceiveData(m_job, out.data(), out.size(), 0);
        
        base64Decode(data.latin1().data(), data.latin1().length(), out);
        
        response.setResponseFired(true);
        d->client()->didReceiveData(m_job, out.data(), out.size(), 0);        
    } else {
        // We have to convert to UTF-16 early due to limitations in KURL
        data = decodeURLEscapeSequences(data, TextEncoding(charset));
        response.setTextEncodingName("UTF-16");
        d->client()->didReceiveResponse(m_job, response);
        if (data.length() > 0)
            d->client()->didReceiveData(m_job, reinterpret_cast<const char*>(data.characters()), data.length() * sizeof(UChar), 0);
    }

    d->client()->didFinishLoading(m_job);
    /*
    
    // old
    
    String header;
    bool base64 = false;

    int index = data.find(',');
    if (index != -1) {
        header = data.substring(5, index - 5).lower();
        data = data.substring(index + 1);

        if (header.endsWith(";base64")) {
            base64 = true;
            header = header.left(header.length() - 7);
        }
    } else
        data = String();

    if (header.isEmpty())
        header = "text/plain;charset=US-ASCII";

    ResourceHandleInternal* d = m_job->getInternal();

    String charset = extractCharsetFromMediaType(header);
    d->m_response.setMimeType(extractMIMETypeFromMediaType(header));
    d->m_response.setHTTPStatusCode(200);

    if (base64) {
        data = decodeURLEscapeSequences(data);
        d->m_response.setTextEncodingName(charset);
        d->client()->didReceiveResponse(m_job, d->m_response);
        
        Vector<char> out;
        if (base64Decode(data.latin1().data(), data.latin1().length(), out) && out.size() > 0)
            d->client()->didReceiveData(m_job, out.data(), out.size(), 0);
    } else {
        // We have to convert to UTF-16 early due to limitations in KURL
        data = decodeURLEscapeSequences(data, TextEncoding(charset));
        d->m_response.setTextEncodingName("UTF-16");
        d->client()->didReceiveResponse(m_job, d->m_response);
        if (data.length() > 0)
            d->client()->didReceiveData(m_job, reinterpret_cast<const char*>(data.characters()), data.length() * sizeof(UChar), 0);
    }

    d->client()->didFinishLoading(m_job);
    */
}

/*
 * This is being called for each HTTP header in the response. This includes '\r\n'
 * for the last line of the header.
 *
 * We will add each HTTP Header to the ResourceResponse and on the termination
 * of the header (\r\n) we will parse Content-Type and Content-Disposition and
 * update the ResourceResponse and then send it away.
 *
 */
size_t CurlHandle::headerCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    CurlHandle* ch = static_cast<CurlHandle*>(data);    

    return ch->receivedHeader(ptr, size, nmemb);    
}

size_t CurlHandle::writeCallback(void* ptr, size_t size, size_t nmemb, void* data)
{
    CurlHandle* ch = static_cast<CurlHandle*>(data);

    return ch->receivedData(ptr, size, nmemb);    
}

size_t CurlHandle::readCallback(void* ptr, size_t size, size_t nmemb, void* data)
{
    CurlHandle* ch = static_cast<CurlHandle*>(data);

    return ch->readData(ptr, size, nmemb);    
}

size_t CurlHandle::receivedHeader(char* ptr, size_t size, size_t nmemb)
{
    if (m_cancelled)
        return 0;
	
    ResourceHandleInternal* d = m_job->getInternal();
    
    size_t totalSize = size * nmemb;
    ResourceHandleClient* client = d->client();
    if (!client)
        return totalSize;

    String header(static_cast<const char*>(ptr), totalSize);
    
    /*
     * a) We can finish and send the ResourceResponse
     * b) We will add the current header to the HTTPHeaderMap of the ResourceResponse
     */
    if (header == String("\r\n")) {
        CURLcode err;

        double contentLength = 0;
        err = curl_easy_getinfo(m_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
        d->m_response.setExpectedContentLength(static_cast<long long int>(contentLength));

        const char* hdr(NULL);
        err = curl_easy_getinfo(m_handle, CURLINFO_EFFECTIVE_URL, &hdr);
		if (CURLE_OK == err) {
        	d->m_response.setURL(KURL(ParsedURLString,hdr));
		}

        long httpCode = 0;
        err = curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &httpCode);
        d->m_response.setHTTPStatusCode(httpCode);
		m_httpStatus = httpCode;

        d->m_response.setMimeType(extractMIMETypeFromMediaType(d->m_response.httpHeaderField("Content-Type")));
        d->m_response.setTextEncodingName(extractCharsetFromMediaType(d->m_response.httpHeaderField("Content-Type")));
        d->m_response.setSuggestedFilename(filenameFromHTTPContentDisposition(d->m_response.httpHeaderField("Content-Disposition")));

        // HTTP redirection
        if (httpCode >= 300 && httpCode < 400) {
            String location = d->m_response.httpHeaderField("location");
            if (!location.isEmpty()) {
                KURL newURL = KURL(m_job->firstRequest().url(), location);

                // For a redirect, the cookies need to be corrected for the new ones
                if (m_cookie) {
                    free(m_cookie);
                    m_cookie = 0;
                }

                // Set cookies for the new url URL
                String cookieStr = cookies( d->m_document, newURL);
                if (cookieStr.isEmpty()) {
                    cookieStr = "";
                }
            
                m_cookie = strdup(cookieStr.utf8().data());
                curl_easy_setopt(m_handle, CURLOPT_COOKIE, m_cookie);

				// Might have been disabled if accepting certificate so reenable.
				curl_easy_setopt(m_handle, CURLOPT_SSL_VERIFYPEER, 1L);
                
                // If there is authorization required, it needs to be corrected as well
                m_authHandler->addCredentials(newURL, d->m_user, d->m_pass);
                
                ResourceRequest redirectedRequest = m_job->firstRequest();
                redirectedRequest.setURL(newURL);
                if (client) {
                    client->willSendRequest(m_job, redirectedRequest, d->m_response);

                    // We could be cancelled at this point
                    if (m_cancelled)
                        return 0;                    
                }

                d->m_firstRequest.setURL(newURL);

                return totalSize;
            }
        }
        else if (httpCode == 401 && m_authHandler->handleChallenge()) {
            // If we handle the auth challenge, it means we are going to try again.
            // In this case don't send any info to the client.
            return totalSize;
        }
        else if (httpCode == 100) {
            return totalSize;
        }

		if( m_cacheEntry ) 
		{
			m_cacheEntry->m_url = d->m_response.url().string();
			m_cacheEntry->m_statusCode = d->m_response.httpStatusCode();
			m_cacheEntry->m_mimeType = d->m_response.mimeType();
			m_cacheEntry->m_fileName = d->m_response.suggestedFilename();
			m_cacheEntry->m_encodingName = d->m_response.textEncodingName();
			m_cacheEntry->m_expectedContentLength = d->m_response.expectedContentLength();
		}

        if (client && !d->m_response.responseFired()) {
            client->didReceiveResponse(m_job, d->m_response);
            // We could be cancelled at this point
            if (m_cancelled)
                return 0;
        }
        d->m_response.setResponseFired(true);

    } else {
		
		bool canCache =true;
		int splitPos = header.find(":");
        if (splitPos != -1)
		{
			// It's more efficient to process the headers here rather than everytime the
			// framework calls doUpdateResourceResponse().
			String name=header.left(splitPos).stripWhiteSpace();
			String value=header.substring(splitPos+1).stripWhiteSpace();
			if( equalIgnoringCase( name, "Cache-Control") )
			{
				HeaderValues values(value);

				if( values.contains("no-cache") || values.contains("no-store") || values.contains("must-revalidate") ) {
					// moved to ResourceResponseBase.cpp d->m_response.setExpirationDate(time(NULL));
					canCache=false;
				}
		
				int agesecs = -1;
				const String& val = values.getValue("max-age");
				if (!val.isEmpty())
					agesecs = val.toInt();
				else {
					const String& sval = values.getValue("s-maxage");
					if (!sval.isEmpty())
						agesecs = sval.toInt();
				}
				if (agesecs > 0) {
					time_t extime = time(0) + agesecs;
					if (PalmBrowserSettings()->diskCacheDebug)
						g_debug("max-age/s_maxage adds %d seconds, so expiration is %lu", agesecs, extime);
					if( m_cacheEntry ) {
						m_cacheEntry->m_expiresAt = extime;
					}
				} else if( !agesecs ) {
					canCache=false; // max-age=0
				} 
			}
			else if( name==String("Pragma") )
			{
				if( value.contains("no-cache",false) || value.contains("no-store",false) ) {
					// moved to ResourceResponseBase.cpp d->m_response.setExpirationDate(time(NULL));
					canCache = false;
				}
			}
			else if( name == String("Expires") )
			{
				time_t exTime;
				if( equalIgnoringCase( value, "NOW" ) )
					exTime = time(0);
				else
					exTime = curl_getdate( value.utf8().data(), 0 );
				// moved to ResourceResponseBase.cpp d->m_response.setExpirationDate( exTime );
				if( m_cacheEntry ) m_cacheEntry->m_expiresAt=exTime;
			}
            else if (name.lower() == "set-cookie") {

                if (!value.isEmpty()) {

                    // RFC2109: Maximum cookie length we are expected to handle is
                    // 4096 bytes long. Otherwise trim. We set this to slightly larger
                    if (value.length() > maxCookieLen) {
                        value.truncate(maxCookieLen);
                    }

                    if(m_job->firstRequest().allowCookies()) {
                        const char* url;
                        (void) curl_easy_getinfo(m_handle, CURLINFO_EFFECTIVE_URL, &url);
                        WebCore::setCookies( d->m_document, KURL(ParsedURLString,url), value);
                    }
                }
        
                // throw out all the cookies being managed by libcurl
                curl_easy_setopt(m_handle, CURLOPT_COOKIELIST, "ALL");
            }
                
            d->m_response.setHTTPHeaderField(name, value);
		}
		
		if( !canCache ) {
			//const char* url;
            //(void) curl_easy_getinfo(m_handle, CURLINFO_EFFECTIVE_URL, &url);
			//printf( "DISK-CACHE : cancelling cache entry for %s\n", url ); 
			delete m_cacheEntry;
			m_cacheEntry=0;
		}
		if( m_cacheEntry ) m_cacheEntry->addHeader(header);
    }

    return totalSize;
}

void CurlHandle::beginReceiveData()
{
    if (m_cancelled)
        return;
    
    m_bufferedData.clear();    
}

void CurlHandle::commitReceiveData()
{
    ResourceHandleInternal* d = 0;
    long httpCode = 0;
    CURLcode err;
    
    if (m_cancelled) {
        goto Done;
    }

    if (!m_bufferedData.size()) {
        goto Done;
    }
    
    d = m_job->getInternal();
    
    // this shouldn't be necessary but apparently is. CURL writes the data
    // of html page even if it is a redirect that was handled internally
    // can be observed e.g. on gmail.com
    httpCode = 0;
    err = curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &httpCode);
    if (CURLE_OK == err && httpCode >= 300 && httpCode < 400) {
        goto Done;
    }

    // Only when running in browserserver we want to clear buffered data since
    // we're going to try again.
    if (CURLE_OK == err
        && httpCode == 401
        && !m_authHandler->cancelled()
        && PalmBrowserSettings()->runningInBrowserServer)
        goto Done;

    if (CURLE_OK == err && httpCode == 100) {
        goto Done;
    }

    // since the code in headerCallback will not have run for local files
    // the code to set the URL and fire didReceiveResponse is never run,
    // which means the ResourceLoader's response does not contain the URL.
    // Run the code here for local files to resolve the issue.
    // TODO: See if there is a better approach for handling this.
    if (!d->m_response.responseFired()) {
        const char* url(NULL);
        err = curl_easy_getinfo(m_handle, CURLINFO_EFFECTIVE_URL, &url);
		if (CURLE_OK == err) {
        	d->m_response.setURL(KURL(ParsedURLString,url));
		}
        if (d->client()) {
            d->client()->didReceiveResponse(m_job, d->m_response);
            // We could be cancelled at this point
            if (m_cancelled)
                goto Done;
        }            
        d->m_response.setResponseFired(true);
    }

	if (m_cacheEntry) {
	    if (!m_cacheEntry->addData((const unsigned char*) m_bufferedData.data(), (int) m_bufferedData.size())) {
	        delete m_cacheEntry;
	        m_cacheEntry = 0;
	    }
	}

    if (d->client())
        d->client()->didReceiveData(m_job, m_bufferedData.data(), m_bufferedData.size(), 0);

Done:

    m_bufferedData.clear();
}

size_t CurlHandle::receivedData(void* ptr, size_t size, size_t nmemb)
{
    if (m_cancelled)
        return 0;
    
    m_bufferedData.append(static_cast<char*>(ptr), size * nmemb);
	
    return size * nmemb;
}

size_t CurlHandle::readData(void* ptr, size_t size, size_t nmemb)
{
    if (m_cancelled)
        return 0;

    //ResourceHandleInternal* d = m_job->getInternal();

    size_t sent = 0;
    size_t toSend = size * nmemb;
    if (!toSend)
        return 0;

	ASSERT(m_job->firstRequest().httpBody() != NULL);
    Vector<FormDataElement> elements = m_job->firstRequest().httpBody()->elements();
    if (m_formDataElementIndex >= elements.size())
        return 0;

    FormDataElement element = elements[m_formDataElementIndex];

    if (element.m_type == FormDataElement::encodedFile) {
        if (!m_file)
            m_file = fopen(element.m_filename.utf8().data(), "rb");

        if (!m_file) {
            // FIXME: show a user error?
#ifndef NDEBUG
            printf("Failed while trying to open %s for upload\n", element.m_filename.utf8().data());
#endif
            m_job->cancel();
            return 0;
        }

        sent = fread(ptr, size, nmemb, m_file);
        if (!size && ferror(m_file)) {
            // FIXME: show a user error?
#ifndef NDEBUG
            printf("Failed while trying to read %s for upload\n", element.m_filename.utf8().data());
#endif
            m_job->cancel();
            return 0;
        }
        if (feof(m_file)) {
            fclose(m_file);
            m_file = 0;
            m_formDataElementIndex++;
        }
    } else {
        size_t elementSize = element.m_data.size() - m_formDataElementDataOffset;
        sent = elementSize > toSend ? toSend : elementSize;
        memcpy(ptr, element.m_data.data() + m_formDataElementDataOffset, sent);
        if (elementSize > sent)
            m_formDataElementDataOffset += sent;
        else {
            m_formDataElementDataOffset = 0;
            m_formDataElementIndex++;
        }
    }

    return sent;
}


void CurlHandle::finishedLoading(int httpCode)
{
    ResourceHandleInternal* d = m_job->getInternal();

    if (httpCode == 401 && !m_authHandler->cancelled()) {
        // In case that we're running in an app and we don't have user
        // interaction to provide right credentials immediately or cancel
        // authentication we cancel the job when receiving error 401.
        // Otherwise we end up in a loop. See DFISH-17874.
        if (!PalmBrowserSettings()->runningInBrowserServer) {
            if (d->client())
                d->client()->didFinishLoading(m_job);
            CurlHandlePool::instance()->cancel(this);
            return;
        } else {
            // We are going to reuse the job
            CurlHandlePool::instance()->restartAsync(this);
            return;
        }
    }

	if( m_cacheEntry && httpCode != 304 ) // 304 = not modified so payload will be emtpy - don't cache.
		diskCache()->add( m_cacheEntry );
	m_cacheEntry=0;
	
    if (d->client())
        d->client()->didFinishLoading(m_job);
	
    delayedDelete();
}

/**
 * Handle a failure loading a resource.
 *
 * @param errorCode The CURL error code when trying to load a resource.
 * @param errorCodeString The non-localized error message.
 */
void CurlHandle::failedLoading(CURLcode errorCode, const String& errorCodeString)
{
    ResourceHandleInternal* d = m_job->getInternal();

    char* url = 0;
	long httpcode = 0;
	long errNo = 0;
    long httpResponseCode = 0;
    String strUrl;
    int err;

    err = curl_easy_getinfo(m_handle, CURLINFO_EFFECTIVE_URL, &url);
	if (CURLE_OK == err) {
		strUrl = url;
	}
    curl_easy_getinfo(m_handle, CURLINFO_HTTP_CONNECTCODE, &httpcode );
    curl_easy_getinfo(m_handle, CURLINFO_OS_ERRNO, &errNo );
    curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &httpResponseCode);
    
    //fprintf(stderr,"Curl ERROR for url=[%s], httpcode=%ld, responseCode=%ld, error: %d\n",
    //        url, httpcode, httpResponseCode, errorCode);

    if (httpResponseCode == 401 && !m_authHandler->cancelled() &&
        errorCode == CURLE_OPERATION_TIMEDOUT) {

        // Timed out while waiting for user/password entry from user. we
        // are gonna retry (with the updated authentication credentials)
        CurlHandlePool::instance()->restartAsync(this);
        return;
    }
	
	if (!strUrl.isEmpty()) {
		d->m_response.setURL( KURL(ParsedURLString,strUrl) );
	}
	d->m_response.setHTTPStatusCode( httpResponseCode );
    
    if (d->client()) {
		int palmErrNo(Palm::ERR_CURL_FAILURE);
		if (0 != errNo) {
			palmErrNo = Palm::MapErrnoToPalmError(errNo);
		}
		else if (CURLE_OK != errorCode) {
			palmErrNo = Palm::MapCurlErrToPalmError(errorCode);
		}

        d->client()->didFail(m_job, ResourceError(strUrl, palmErrNo,
                                                  strUrl, errorCodeString));
	}

    delayedDelete();
}

void CurlHandle::cancelledLoading()
{
    delayedDelete();
}

void CurlHandle::abortLoading()
{
	ResourceHandleInternal* d = m_job->getInternal();

	d->m_response.setMimeType("unknown");
	d->m_response.setHTTPStatusCode(400);

	d->client()->didReceiveResponse(m_job, d->m_response);

	d->client()->didFinishLoading(m_job);
}

void CurlHandle::asyncTimerCallback(Timer<CurlHandle>*)
{
	if (shouldAbort()) {
		abortLoading();
        delayedDelete();
        return;
	}

    KURL kurl = m_job->firstRequest().url();
    if (equalIgnoringCase(kurl.protocol(), "data")) {
        parseDataUrl();
        delayedDelete();
        return;
    }

	if (equalIgnoringCase(kurl.protocol(), "file")) {
        CurlHandlePool* pool = CurlHandlePool::instance();
		
		m_handle = pool->acquire(this);
		if (!setupJob()) {
			delayedDelete();
			return;
		}

		pool->startAsync(this);
        return;
    }
}

void CurlHandle::delayedDelete()
{
    if (!m_delTimer.isActive())
        m_delTimer.startOneShot(0);
}

void CurlHandle::delTimerCallback(Timer<CurlHandle>*)
{
    delete this;    
}

void CurlHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    m_authHandler->receivedUserCredentials(challenge, credential);
}

void CurlHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    m_authHandler->receivedUserCancellation(challenge);
}

void CurlHandle::addAppendedHTTPHeader( const char* name, const char* value )
{
	sAppendedHttpHeaders[ std::string(name)] = std::string(value) ;
}

/**
 * Remove the appended HTTP header value.
 */
void CurlHandle::removeAppendedHTTPHeader( const char* name )
{
	sAppendedHttpHeaders.erase(std::string(name));
}

void CurlHandle::useHttpProxy(const String& host, const int port)
{
    m_httpProxyEnabled = true;
    m_httpProxyHost = host;
    m_httpProxyPort = port;
}

void CurlHandle::useNetworkInterface(const String& interfaceName)
{
	m_forceNetworkInterfaceEnabled = true;
	m_networkInterfaceName = interfaceName;
}

void CurlHandle::useDNSServers(const String& dnsServers)
{
    if(!dnsServers.isEmpty()) {
        m_forceDNSServersEnabled = true;
        m_dnsServers = dnsServers;
    }
}


}
