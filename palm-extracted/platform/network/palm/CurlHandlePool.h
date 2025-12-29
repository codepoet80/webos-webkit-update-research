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
 * 
 * Derived from glibcurl. Copyright message follows
 *
 * |_) /|  Copyright (C) 2004  |  richard@
 * | \/¯|  Richard Atterer     |  atterer.net
 * ¯ '` ¯
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization of the
 * copyright holder.
 */

#ifndef CURLHANDLEPOOL_H
#define CURLHANDLEPOOL_H

#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

#include <curl/curl.h>
#include <glib.h>
#include <time.h>

#include <list>

namespace WebCore {

class CurlHandle;
class CredentialsCache;
class KURL;

class CurlHandlePool : Noncopyable {
public:

    static CurlHandlePool* instance();

	void restart();

    CURL* acquire(CurlHandle* job);
    void  release(CURL* handle);

    void startAsync(CurlHandle* job);
    void startSync(CurlHandle* job);
    void cancel(CurlHandle* job);
    void restartAsync(CurlHandle* job);

    CredentialsCache* credentialsCache() const;

	
	void watchHandle( CurlHandle* h );
	void unwatchHandle( CurlHandle* h );
	
private:

    CurlHandlePool();
    ~CurlHandlePool();

    void startJob(CurlHandle* job, bool sync);
    void cancelJob(CurlHandle* job);
    bool removeQueuedJob(CurlHandle* job);
    int startQueuedJobs();
    bool startOrQueueJob(CurlHandle* job);
    
    static void     registerUnregisterFds();
    static gboolean sourcePrepare(GSource* source, gint* timeout);
    static gboolean sourceCheck(GSource* source);
    static gboolean sourceDispatch(GSource* source, GSourceFunc callback,
                                   gpointer userData);
    
    static GSourceFuncs m_srcFuncs;

	typedef  std::list<CurlHandle*> HandleList;
	static HandleList sHandles;
	
#define GLIBCURL_FDMAX 2048
    
    GSource* m_source;
    GPollFD  m_lastPollFd[GLIBCURL_FDMAX + 1];
    int      m_lastPollFdMax;

    bool     m_handleAdded;
	
    fd_set   m_fdRead;
    fd_set   m_fdWrite;
    fd_set   m_fdExc;
    int      m_fdMax;
    
    CURLM*   m_multiHandle;
    CURLSH*  m_shareHandle;
    bool     m_withinMulti;

    std::list<CurlHandle*> m_queuedJobs;
    int                    m_activeJobs;
    
    int             m_currTimeout;
    struct timespec m_timeAtLastDispatch;

    CredentialsCache* m_credentialsCache;
};

}

    

#endif /* CURLHANDLEPOOL_H */
