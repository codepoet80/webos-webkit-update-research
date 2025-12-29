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
 * ============================================================ */

#include "config.h"
#include "CurlHandlePool.h"

#include "CredentialsCache.h"
#include "CurlHandle.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "palmwebglobal.h"

#include "webkitpalmsettings.h"

#include <list>
#include <vector>



#undef DEBUG_MESSAGES

#ifdef DEBUG_MESSAGES
#define CTRACE(...) {printf("CurlHandlePool: " __VA_ARGS__); fflush(stdout);}
#else
#define CTRACE(...) (void)0
#endif

/* Timeout for the fds passed to glib's poll() call, in millisecs.
   curl_multi_fdset(3) says we should call curl_multi_perform() at regular
   intervals. */
#define GLIBCURL_TIMEOUT 100

/* GIOCondition event masks */
#define GLIBCURL_READ  (G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP)
#define GLIBCURL_WRITE (G_IO_OUT | G_IO_ERR | G_IO_HUP)
#define GLIBCURL_EXC   (G_IO_ERR | G_IO_HUP)


namespace WebCore
{

static CurlHandlePool* gInstance = 0;

GSourceFuncs CurlHandlePool::m_srcFuncs = {
    &CurlHandlePool::sourcePrepare,
    &CurlHandlePool::sourceCheck,
    &CurlHandlePool::sourceDispatch,
    0,
    0,
    0
};

CurlHandlePool* CurlHandlePool::instance()
{
    if (!gInstance)
        new CurlHandlePool();
    return gInstance;
}

CurlHandlePool::CurlHandlePool()
    : m_source(0)
    , m_fdMax(-1)
    , m_handleAdded(false)
    , m_withinMulti(false)
    , m_credentialsCache(new CredentialsCache)
{
    gInstance = this;

    if (CURLE_OK != curl_global_init(CURL_GLOBAL_ALL))
        g_error("Cannot initialize cURL");

    m_multiHandle = curl_multi_init();
    m_shareHandle = curl_share_init();
    curl_share_setopt(m_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(m_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
	
	curl_multi_setopt( m_multiHandle, CURLMOPT_MAXCONNECTS, 40);
	//curl_multi_setopt( m_multiHandle, CURLMOPT_PIPELINING, 1 );
	
    m_source = g_source_new(&m_srcFuncs, sizeof(GSource));
    g_source_attach(m_source, g_main_loop_get_context(webkit_palm_get_mainloop()));

    memset(&m_lastPollFd, 0, sizeof(m_lastPollFd));
    for (int i = 1; i <= GLIBCURL_FDMAX; ++i) {
        m_lastPollFd[i].fd = i;
        m_lastPollFd[i].events = 0;
        m_lastPollFd[i].revents = 0;
    }
    m_lastPollFdMax = 0;
    m_timeAtLastDispatch.tv_sec = 0;
    m_timeAtLastDispatch.tv_nsec = 0;
    m_currTimeout = 0;
    
    m_activeJobs = 0;
}

CurlHandlePool::~CurlHandlePool()
{
    curl_multi_cleanup(m_multiHandle);
    curl_share_cleanup(m_shareHandle);

    delete m_credentialsCache;

    gInstance = 0;
}

void CurlHandlePool::restart()
{
    // Should not happen
    ASSERT(!m_withinMulti);

	// First remove all easy handles sitting in the multi handle
    for (HandleList::iterator it = sHandles.begin(); it != sHandles.end(); ++it) {
		CurlHandle* job = (*it);
		if (job->isInMulti())
			curl_multi_remove_handle(m_multiHandle, job->m_handle);
    }

	// Now we can clean up the multi handle
    curl_multi_cleanup(m_multiHandle);

	// Re-initialize the multi handle
    m_multiHandle = curl_multi_init();
	curl_multi_setopt( m_multiHandle, CURLMOPT_MAXCONNECTS, 40 );
	//curl_multi_setopt( m_multiHandle, CURLMOPT_PIPELINING, 1 );
		
    memset(&m_lastPollFd, 0, sizeof(m_lastPollFd));
    for (int i = 1; i <= GLIBCURL_FDMAX; ++i) {
        m_lastPollFd[i].fd = i;
        m_lastPollFd[i].events = 0;
        m_lastPollFd[i].revents = 0;
    }
    m_lastPollFdMax = 0;
    m_timeAtLastDispatch.tv_sec = 0;
    m_timeAtLastDispatch.tv_nsec = 0;
    m_currTimeout = 0;    

    // Now add back all handles which have not been canceled
    for (HandleList::iterator it = sHandles.begin(); it != sHandles.end(); ++it) {
		CurlHandle* job = (*it);
		if (job->isInMulti()) {

			if (job->m_cancelled) {

				if (job->m_handle) {
					curl_easy_cleanup(job->m_handle);
					job->m_handle = 0;
					m_activeJobs--;
				}
				
				cancelJob(job);
			}
			else {
				
				curl_multi_add_handle(m_multiHandle, job->m_handle);
				job->setInMulti(true);
				m_handleAdded = true;
			}
        }
    }

	// we may have canceled some jobs. see if we can start other jobs
	startQueuedJobs();

    // wake up main loop if it is suspended in a poll
    g_main_context_wakeup(g_main_loop_get_context(webkit_palm_get_mainloop()));	
}

CURL* CurlHandlePool::acquire(CurlHandle* job)
{
    CURL* handle = curl_easy_init();
    curl_easy_setopt(handle, CURLOPT_SHARE, m_shareHandle);

    return handle;
}

void CurlHandlePool::release(CURL* handle)
{
    if (!m_withinMulti) 
    {
        CurlHandle* job = NULL;
        CURLcode err = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &job);

        if (err == CURLE_OK && job && job->isInMulti()) 
        {
            curl_multi_remove_handle(m_multiHandle, handle);

            registerUnregisterFds();
        
            // this job is no longer active, so we may need to start a pending job
            m_activeJobs--;

            CTRACE("%s: %s: releasing job. Num active: %d, num queued: %d\n", 
                    __FILE__, __FUNCTION__, m_activeJobs, m_queuedJobs.size());

            startQueuedJobs();
        } else {
            CTRACE("%s: %s: job was not in multi, so nothing to do.\n",
                    __FILE__, __FUNCTION__);
        }

    }
    else {
        // FIXME: Not implemented
        notImplemented();
    }
}

void CurlHandlePool::startAsync(CurlHandle* job)
{
    if (m_withinMulti) {
        // deferred start
        m_queuedJobs.push_back(job);
    }
    else {
        // start immediately, or queue if too many active jobs
        startOrQueueJob(job);
    }
}

void CurlHandlePool::startSync(CurlHandle* job)
{
    CURLcode err = curl_easy_perform(job->m_handle);
    if (CURLE_OK == err) {
        long httpCode = 0;
        curl_easy_getinfo(job->m_handle, CURLINFO_RESPONSE_CODE, &httpCode);
        job->finishedLoading(httpCode);
    }
    else {
        job->failedLoading(err, String(curl_easy_strerror(err)));
    }
}

void CurlHandlePool::cancel(CurlHandle* job)
{
    if (removeQueuedJob(job))
        return;
    
    if (m_withinMulti) {
        // deferred delete. FIXME: do we need to do anything here?
    }
    else {
        // close immediately
        cancelJob(job);
    }
}

/**
 * @brief Starts job immediately if number of active connections is below max.
 *        Otherwise, queues job.
 * 
 * @return True if job was started immediately.
 * 
 */
bool CurlHandlePool::startOrQueueJob(CurlHandle* job) 
{
    bool startedJob = false;
    
    // start job immediately if number of active jobs is below max
    if (m_activeJobs < PalmBrowserSettings()->maxActiveConnections) 
    {
        startJob(job, false);
        CTRACE("%s: %s: starting job because active jobs: %d\n", 
                __FILE__, __FUNCTION__, m_activeJobs);
        startedJob = true;

    } else {

        m_queuedJobs.push_back(job);
        CTRACE("%s: %s: queuing job\n", __FILE__, __FUNCTION__);
        startedJob = false;
    }    
    
    return startedJob;
}

/**
 * @brief Starts queued jobs up to the maximum number of active connections.
 * 
 * @return Number of started jobs.
 * 
 */
int CurlHandlePool::startQueuedJobs()
{
    if (m_queuedJobs.empty()) {
        return 0;
    }
    
    int startedJobs = 0;
    
    while (m_activeJobs < PalmBrowserSettings()->maxActiveConnections
		 && m_queuedJobs.size() ) 
    {
        CurlHandle* job = m_queuedJobs.front();
        m_queuedJobs.pop_front();

        startJob(job, false);
        startedJobs++;
        
    }
    
    return startedJobs;
}

bool CurlHandlePool::removeQueuedJob(CurlHandle* job)
{
    for (std::list<CurlHandle*>::iterator it = m_queuedJobs.begin();
        it != m_queuedJobs.end(); it++)
    {
        if (*it == job) {
            m_queuedJobs.erase(it);
            cancelJob(job);
            return true;
        }
    }
    
    return false;
}

void CurlHandlePool::restartAsync(CurlHandle* job)
{
    // Should not happen
    ASSERT(!m_withinMulti);

    curl_multi_remove_handle(m_multiHandle, job->m_handle);
    curl_multi_add_handle(m_multiHandle, job->m_handle);

    m_handleAdded = true;
    
    // wake up main loop if it is suspended in a poll
    g_main_context_wakeup(g_main_loop_get_context(webkit_palm_get_mainloop()));
}

CredentialsCache* CurlHandlePool::credentialsCache() const
{
    return m_credentialsCache;    
}

void CurlHandlePool::startJob(CurlHandle* job, bool sync)
{
    curl_multi_add_handle(m_multiHandle, job->m_handle);

    job->setInMulti(true);
    
    m_handleAdded = true;

    m_activeJobs++;
    
    CTRACE("%s: %s: started job. Active: %d, Queued: %d\n", 
            __FILE__, __FUNCTION__, m_activeJobs, m_queuedJobs.size());
    
    // wake up main loop if it is suspended in a poll
    g_main_context_wakeup(g_main_loop_get_context(webkit_palm_get_mainloop()));
}

void CurlHandlePool::cancelJob(CurlHandle* job)
{
    job->cancelledLoading();
}

void CurlHandlePool::registerUnregisterFds()
{
    CurlHandlePool* pool = CurlHandlePool::instance();

      int fd, fdMax;

      FD_ZERO(&pool->m_fdRead);
      FD_ZERO(&pool->m_fdWrite);
      FD_ZERO(&pool->m_fdExc);
      pool->m_fdMax = -1;

      // What fds does libcurl want us to poll?
      curl_multi_fdset(pool->m_multiHandle, &pool->m_fdRead,
                       &pool->m_fdWrite, &pool->m_fdExc, &pool->m_fdMax);
      ASSERT(pool->m_fdMax >= -1 && pool->m_fdMax <= GLIBCURL_FDMAX);

      fdMax = pool->m_fdMax;
      if (fdMax < pool->m_lastPollFdMax) fdMax = pool->m_lastPollFdMax;

      // Has the list of required events for any of the fds changed? 
      for (fd = 0; fd <= fdMax; ++fd) {
          gushort events = 0;
          if (FD_ISSET(fd, &pool->m_fdRead))  events |= GLIBCURL_READ;
          if (FD_ISSET(fd, &pool->m_fdWrite)) events |= GLIBCURL_WRITE;
          if (FD_ISSET(fd, &pool->m_fdExc))   events |= GLIBCURL_EXC;

          // List of events unchanged => no (de)registering 
          if (events == pool->m_lastPollFd[fd].events) continue;

          // fd is already a lastPollFd, but event type has changed => do nothing.
          //   Due to the implementation of g_main_context_query(), the new event
          //   flags will be picked up automatically. 
          if (events != 0 && pool->m_lastPollFd[fd].events != 0) {
              pool->m_lastPollFd[fd].events = events;
              continue;
          }
          pool->m_lastPollFd[fd].events = events;

          // Otherwise, (de)register as appropriate 
          if (events == 0) {
              g_source_remove_poll(pool->m_source, &pool->m_lastPollFd[fd]);
              pool->m_lastPollFd[fd].revents = 0;
          } else {
              g_source_add_poll(pool->m_source, &pool->m_lastPollFd[fd]);
          }
      }

      pool->m_lastPollFdMax = pool->m_fdMax;
}

/**
 *
 * From glib's docs:
 *
 * Called before all the file descriptors are polled. If the source can determine
 * that it is ready here (without waiting for the results of the poll() call)
 * it should return TRUE. It can also return a timeout_ value which should be
 * the maximum timeout (in milliseconds) which should be passed to the poll()
 * call. The actual timeout used will be -1 if all sources returned -1, or it
 * will be the minimum of all the timeout_ values returned which were >= 0. 
 *
 * @see http://library.gnome.org/devel/glib/unstable/glib-The-Main-Event-Loop.html#GSourceFuncs
 *
 * @return TRUE if this source is ready or FALSE if not.
 */
gboolean CurlHandlePool::sourcePrepare(GSource* source, gint* timeout)
{
    CurlHandlePool* pool = CurlHandlePool::instance();

    if (pool->m_handleAdded) {
        pool->m_currTimeout = 0;
        *timeout = pool->m_currTimeout;
        return TRUE;
    }
        
    long curlTimeout = 0;
    curl_multi_timeout(pool->m_multiHandle, &curlTimeout);

    if (curlTimeout == 0) {
        pool->m_currTimeout = 0;
        *timeout = pool->m_currTimeout;
        return TRUE;
    }

    if (curlTimeout < 0) {
        pool->m_currTimeout = sHandles.empty() ? -1 : GLIBCURL_TIMEOUT;
        *timeout = pool->m_currTimeout;
        return FALSE;
    }

    pool->m_currTimeout = MIN(curlTimeout, GLIBCURL_TIMEOUT);
    *timeout = pool->m_currTimeout;
    return FALSE;
}

/**
 * Called after all the file descriptors are polled.
 *
 * @return The source should return TRUE if it is ready to be dispatched.
 * Note that some time may have passed since the previous prepare function
 * was called, so the source should be checked again here. 
 */
gboolean CurlHandlePool::sourceCheck(GSource* source)
{
    CurlHandlePool* pool = CurlHandlePool::instance();

    if (sHandles.empty())
        return FALSE;
    
    if (pool->m_handleAdded)
        return TRUE;

    for (int fd = 0; fd <= pool->m_fdMax; ++fd) {
        if (pool->m_lastPollFd[fd].revents != 0)
            return TRUE;
    }

    struct timespec currTime;
    clock_gettime(CLOCK_MONOTONIC, &currTime);

    // Curl wants us to call it regularly even if there is no data available
    if (((currTime.tv_sec - pool->m_timeAtLastDispatch.tv_sec) * 1000 +
         (currTime.tv_nsec - pool->m_timeAtLastDispatch.tv_nsec) / 1000000) >=
        pool->m_currTimeout) {
               
        return TRUE;
    }
        
    return FALSE;

/*    
    int fd;
    bool somethingHappened = false;
  
    FD_ZERO(&pool->m_fdRead);
    FD_ZERO(&pool->m_fdWrite);
    FD_ZERO(&pool->m_fdExc);
    for (fd = 0; fd <= pool->m_fdMax; ++fd) {
        gushort revents = pool->m_lastPollFd[fd].revents;
        if (revents == 0) continue;
        somethingHappened = true;
        if (revents & (G_IO_IN | G_IO_PRI))
            FD_SET((unsigned)fd, &pool->m_fdRead);
        if (revents & G_IO_OUT)
            FD_SET((unsigned)fd, &pool->m_fdWrite);
        if (revents & (G_IO_ERR | G_IO_HUP))
            FD_SET((unsigned)fd, &pool->m_fdExc);
    }

    if (pool->m_handleAdded) {
        pool->m_handleAdded = false;
        return TRUE;
    }

    if (somethingHappened) {
        return TRUE;
    }

    return FALSE;
*/
}

/**
 * Called to dispatch the event source, after it has returned TRUE in either its prepare or
 * its check function. The dispatch function is passed in a callback function and data.
 * The callback function may be NULL if the source was never connected to a callback using
 * g_source_set_callback(). The dispatch function should call the callback function with
 * user_data and whatever additional parameters are needed for this type of event source. 
 */
gboolean CurlHandlePool::sourceDispatch(GSource* source, GSourceFunc callback,
                                        gpointer userData)
{
    CURLMcode ret;

    CurlHandlePool* pool = CurlHandlePool::instance();

    clock_gettime(CLOCK_MONOTONIC, &pool->m_timeAtLastDispatch);
    
    pool->m_handleAdded = false;    


    for (HandleList::const_iterator it = sHandles.begin(); it != sHandles.end(); ++it) {
        (*it)->beginReceiveData();
    }
    
    pool->m_withinMulti = true;
    do {
        int runningJobs = 0;
        ret = curl_multi_perform(pool->m_multiHandle, &runningJobs);        
    } while (ret == CURLM_CALL_MULTI_PERFORM);
    pool->m_withinMulti = false;

    for (HandleList::const_iterator it = sHandles.begin(); it != sHandles.end(); ++it) {
        (*it)->commitReceiveData();
    }
    
    // check the curl messages indicating completed transfers
    // and free their resources
    while (true) {
        int messagesInQueue;
        CURLMsg* msg = curl_multi_info_read(pool->m_multiHandle, &messagesInQueue);
        if (!msg)
            break;

        // find the node which has same d->m_handle as completed transfer
        CURL* handle = msg->easy_handle;
        ASSERT(handle);

        CurlHandle* job = 0;        
        CURLcode err = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &job);
        ASSERT(CURLE_OK == err);
        ASSERT(job);
        if (!job)
            continue;

        if (job->m_cancelled) {
            pool->cancelJob(job);
            continue;
        }

        if (CURLMSG_DONE != msg->msg) 
            continue;
             
        // libCURL returns a CURLE_PARTIAL_FILE error even when it receives the correct
        // number of bytes but the connection closed before receiving the empty last chunk
        // We think this is too strict so we exclude this one case.
        double contentLength(-1);
        double downloadedLength(0);
        curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
        curl_easy_getinfo(handle, CURLINFO_SIZE_DOWNLOAD, &downloadedLength);

        if (CURLE_OK == msg->data.result ||
                (CURLE_PARTIAL_FILE == msg->data.result && contentLength == downloadedLength)) {
            long httpCode = 0;
            curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &httpCode);
            job->finishedLoading(httpCode);
        }
        else {
            job->failedLoading(msg->data.result, String(curl_easy_strerror(msg->data.result)));
        }
    }        
        
    // start any scheduled multi jobs
    pool->startQueuedJobs();

    registerUnregisterFds();
    
    return TRUE;
}

CurlHandlePool::HandleList CurlHandlePool::sHandles;

void CurlHandlePool::watchHandle( CurlHandle* h )
{
	sHandles.push_back(h);
}

void CurlHandlePool::unwatchHandle( CurlHandle* h )
{
	HandleList::iterator it = std::find( sHandles.begin(), sHandles.end(), h );
	if( it != sHandles.end() )
		sHandles.erase(it);
}

}
