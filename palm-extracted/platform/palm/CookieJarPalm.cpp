/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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
#include "CookieJar.h"

#include "KURL.h"
#include "PlatformString.h"
#include "CString.h"
#include "StringHash.h"
#include "Cookie.h"
#include "Document.h"
#include "Page.h"
#include "CookieServicePalm.h"
#include "palmwebpage.h"
#include "palmwebframeclient.h"
#include "palmwebframe.h"
#include "palmwebframeprivate.h"
#include "webkitpalmsettings.h"
#include <wtf/HashMap.h>

#include <stdio.h>

namespace WebCore {

/*
 * Returns appId if current request is coming from an application context
 * and is not a local file request DFISH-13369
 *
 * This makes sure that XHR requests from applications are secure DFISH-13369
 * If app my.app.com makes a XHR request to amazon.com the url will only
 * contain amazon, but we need to somehow distinguish such requests per app.
 * This is why we append appid to the requested url domain.
 * Final format stored in the db in the "domain-tail" column will be: amazon.com-my.app.com
 * which indicates that these cookies belong to XHR request originating from my.app.com to amazon.com
 */
static String getAppIdDomainTail(const Document* doc, const KURL& url)
{
    String appIdDomainTail;
    if (!PalmBrowserSettings()->runningInBrowserServer && doc && !url.protocol().contains("file", false)) {
        Palm::WebFrame* webFrame = kit(doc->frame());
        if (webFrame && webFrame->client()) {
            Palm::WebFrameClient* webFrameClient = webFrame->client();
            if (webFrameClient) {
                if (const char* appId = webFrameClient->getIdentifier()) {
                    // the process Id is tacked on to the end--get rid of it
                    const char* ptr = appId;
                    while( *ptr && !isspace(*ptr) )
                        ptr++;

                    appIdDomainTail = String(appId, (int)(ptr-appId));
                    if (!appIdDomainTail.isEmpty())
                        appIdDomainTail.insert("-", 0);
                }
            }
        }
    }
    return appIdDomainTail;
}

void setCookies(Document* doc, const KURL& url, const String& value)
{
    //printf("doc=%p %s: %d: %s\n", doc, __PRETTY_FUNCTION__, __LINE__, url.string().utf8().data());

    // Is Accept-Cookies disabled?
    if (!PalmBrowserSettings()->acceptCookies)
        return;
    
    std::string appIdentifier;
    if( PalmBrowserSettings()->runningInBrowserServer && doc )
    {
		Palm::WebFrame* webFrame = kit(doc->frame());
		if (webFrame && webFrame->page()->client()) 
		{
            Palm::WebFrameClient* webFrameClient = webFrame->client();
            if (webFrameClient) {
                if( const char* appId = webFrameClient->getIdentifier() )
                {
                    // the process Id is tacked on to the end--get rid of it
                    const char* ptr = appId;
                    while( *ptr && !isspace(*ptr) )
                        ptr++;

                    appIdentifier = std::string( appId, (int)(ptr-appId) );
                }
            }
		}
    }

    CookieService* service = CookieService::sharedInstance(appIdentifier.c_str());
    if (!service) 
        return;

    String appIdDomainTail = getAppIdDomainTail(doc, url);
    service->setCookies(url, value, appIdDomainTail);
}

String cookies(const Document* doc, const KURL& url)
{
    //printf("doc=%p %s: %d: %s\n", doc, __PRETTY_FUNCTION__, __LINE__, url.string().utf8().data());

    std::string appIdentifier;
    if( PalmBrowserSettings()->runningInBrowserServer && doc )
    {
		Palm::WebFrame* webFrame = kit(doc->frame());
		if (webFrame && webFrame->page()->client()) 
		{
            Palm::WebFrameClient* webFrameClient = webFrame->client();
            if (webFrameClient) {
                if( const char* appId = webFrameClient->getIdentifier() )
                {
                    // the process Id is tacked on to the end--get rid of it
                    const char* ptr = appId;
                    while( *ptr && !isspace(*ptr) )
                        ptr++;

                    appIdentifier = std::string( appId, (int)(ptr-appId) );
                }
            }
		}
    }

    CookieService* service = CookieService::sharedInstance(appIdentifier.c_str());
    if (!service)
        return String();

    String appIdDomainTail = getAppIdDomainTail(doc, url);
    return service->cookies(url, appIdDomainTail);
}

bool cookiesEnabled(const Document* /*document*/ )
{
    //printf("%s: %d\n", __PRETTY_FUNCTION__, __LINE__);
    CookieService* service = CookieService::sharedInstance();
    if (!service) {
        return false;
    }

    return service->cookiesEnabled();
}

bool getRawCookies(const Document*, const KURL&, Vector<Cookie>& rawCookies)
{
    // FIXME: Not yet implemented
    rawCookies.clear();
    return false; // return true when implemented
}

void deleteCookie(const Document*, const KURL&, const String&)
{
    // FIXME: Not yet implemented
}

String cookieRequestHeaderFieldValue(const Document* document, const KURL& url)
{
    // FIXME: include HttpOnly cookie
    return cookies(document, url);
}

}
