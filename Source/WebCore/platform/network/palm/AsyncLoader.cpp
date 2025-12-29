/* ============================================================
 * Date  : 2010-01-01
 * Copyright 2010 Palm, Inc. All rights reserved.
 * ============================================================ */

#include "config.h"
#include "AsyncLoader.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>


#include "palmwebglobal.h"
#include "CString.h"
#include "MIMETypeRegistry.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"

namespace WebCore
{

AsyncLoader* AsyncLoader::instance()
{
	static AsyncLoader* s_instance = 0;
	if (G_UNLIKELY(!s_instance))
		s_instance = new AsyncLoader();
	return s_instance;
}

AsyncLoader::AsyncLoader()
{
    int error = ::pipe(m_mainToThreadPipeFd);
    error = ::pipe(m_threadToMainPipeFd);

	// setup an iochannel on the read end of the pipe
	m_ioChannel =  g_io_channel_unix_new(m_threadToMainPipeFd[0]);
	m_ioSource = g_io_create_watch(m_ioChannel, (GIOCondition) G_IO_IN);
	g_source_set_callback(m_ioSource, (GSourceFunc) threadCallbackOnMainThread, this, NULL);
    g_source_attach(m_ioSource, g_main_loop_get_context(webkit_palm_get_mainloop()));	

	// worker thread
	m_thread = g_thread_create(threadFunction, this, TRUE, NULL);
}

AsyncLoader::~AsyncLoader()
{
	// NO-OP    
}

bool AsyncLoader::load(const String& path, ResourceHandle* handle)
{
	if (!handle)
		return false;

	String filePath = path;
		
	if (filePath.startsWith("file://"))
		filePath = filePath.substring(7);
		
	// Pick off the query string.
	int qo = filePath.find("?");
	if (qo > 0)
		filePath = filePath.left(qo);
		
	filePath = decodeURLEscapeSequences(filePath);		

	AsyncLoaderItem* item = new AsyncLoaderItem(path, filePath, handle);

	// printf("Queuing: %s\n", item->filePath.utf8().data());
	
	m_mutex.lock();
	m_jobQueue.push_back(item);
	m_mutex.unlock();

	wakeupChildThread();

	return true;
}

void AsyncLoader::cancel(ResourceHandle* handle)
{
    m_mutex.lock();

	for (std::list<AsyncLoaderItem*>::iterator it = m_jobQueue.begin();
		 it != m_jobQueue.end(); ) {

		if ((*it)->handle == handle) {
			delete (*it);
			it = m_jobQueue.erase(it);
		}
		else
			++it;
	}

	for (std::list<AsyncLoaderItem*>::iterator it = m_finishedQueue.begin();
		 it != m_finishedQueue.end(); ) {

		if ((*it)->handle == handle) {
			delete (*it);
			it = m_finishedQueue.erase(it);
		}
		else
			++it;
	}	

	m_mutex.unlock();
}

void AsyncLoader::wakeupMainThread()
{
	const char byte = 1;

	m_mutex.lock();
    ssize_t result = ::write(m_threadToMainPipeFd[1], &byte, 1);
	m_mutex.unlock();
}

gpointer AsyncLoader::threadFunction(gpointer arg)
{
	::prctl(PR_SET_NAME, "AsyncLoader", 0, 0, 0);

	AsyncLoader* loader = (AsyncLoader*) arg;

	while (true) {

		AsyncLoaderItem* item = 0;		

		loader->m_mutex.lock();
		if (!loader->m_jobQueue.empty()) {
			item = loader->m_jobQueue.front();
			loader->m_jobQueue.pop_front();
		}
		loader->m_mutex.unlock();

		if (item) {
			loader->load(item);
			continue;
		}

		fd_set rfds;
		int fd = loader->m_mainToThreadPipeFd[0];
		int nfds = fd + 1;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		
		int ret = ::select(nfds, &rfds, NULL, NULL, NULL);
		if (ret <= 0)
			continue;
		else {

			char byte = 0;
			
			loader->m_mutex.lock();			
            ssize_t result = ::read(loader->m_mainToThreadPipeFd[0], &byte, 1);
			loader->m_mutex.unlock();
		}
	}

	return 0;
}

void AsyncLoader::wakeupChildThread()
{
	const char byte = 1;

	m_mutex.lock();
    ssize_t result = ::write(m_mainToThreadPipeFd[1], &byte, 1);
	m_mutex.unlock();    
}

void AsyncLoader::load(AsyncLoaderItem* item)
{
	// const int kMaxFileSizeToLoad = 10 * 1024 * 1024 ; // 10 MB
	
	long bufSize = 0;	
	long readSize = 0;

	FILE *f = ::fopen(item->filePath.utf8().data(), "rb");
	if (!f) {
		item->failed = true;
		goto Done;
	}


	// get file size
	struct stat st;
	if (::stat(item->filePath.utf8().data(), &st) < 0) {
		item->failed = true;
		::fclose(f);
		goto Done;
	}
	bufSize = st.st_size;

	item->bufferSize = bufSize;
	item->buffer = (char*) malloc(bufSize);
	if (item->buffer == NULL) {
		item->failed = true;
		::fclose(f);
		goto Done;
	}

	readSize = fread(item->buffer, sizeof(char), bufSize, f);
	::fclose(f);

	if (bufSize != readSize)
	{
		::free(item->buffer);
		item->buffer = 0;
		item->failed = true;
	}

Done:

	m_mutex.lock();
	m_finishedQueue.push_back(item);
	m_mutex.unlock();

	wakeupMainThread();
}

gboolean AsyncLoader::threadCallbackOnMainThread(GIOChannel* channel, GIOCondition condition, gpointer arg)
{
	AsyncLoader* loader = (AsyncLoader*) arg;
	char byte = 0;

	loader->m_mutex.lock();
	int ret = ::read(g_io_channel_unix_get_fd(channel), &byte, 1);
	loader->m_mutex.unlock();

	loader->oneLoadFinished();

	return TRUE;
}

void AsyncLoader::oneLoadFinished()
{
	AsyncLoaderItem* item = 0;
	
	m_mutex.lock();
	if (!m_finishedQueue.empty()) {
		item = m_finishedQueue.front();
		m_finishedQueue.pop_front();
	}
	m_mutex.unlock();

	if (!item)
		return;

	if (!item->failed) {

		ResourceHandle* handle = item->handle;
		if (handle && handle->client()) {
				
			ResourceResponse rsp;
				
			rsp.setURL(KURL(ParsedURLString,item->path));
			rsp.setHTTPStatusCode(200);
			rsp.setMimeType( MIMETypeRegistry::getMIMETypeForPath(String(item->filePath)) );
			if (rsp.mimeType() == String("text/html") || rsp.mimeType() == String("text/ascii"))
				rsp.setTextEncodingName("UTF-8");
			rsp.setSuggestedFilename(item->path);
			rsp.setExpectedContentLength(item->bufferSize);

			if (!item->handle->getInternal()->m_cancelled)  {
				ResourceRequest redirected(handle->firstRequest()); 
				handle->client()->willSendRequest(handle, redirected, ResourceResponse()); 
				handle->client()->didReceiveResponse(handle, rsp);
				rsp.setResponseFired(true);
				handle->client()->didReceiveData(handle, item->buffer, item->bufferSize, 0);
				handle->client()->didFinishLoading(handle);
			}
		}
	}
	else {

		ResourceHandle* handle = item->handle;
		if (handle && handle->client()) {

			ResourceResponse rsp;
			rsp.setURL(KURL(ParsedURLString,item->path));
			rsp.setHTTPStatusCode(404);
			handle->client()->didReceiveResponse(item->handle, rsp);
			rsp.setResponseFired(true);
			ResourceError err(String(item->path), 404, String(item->path), String("File not found"));
			handle->client()->didFail(item->handle, err);
		}				
	}

	delete item;    
}


AsyncLoader::AsyncLoaderItem::AsyncLoaderItem(const String& _path, const String& _filePath, ResourceHandle* _handle)
	: path(_path)
	, filePath(_filePath)
	, handle(_handle)
	, buffer(0)
	, bufferSize(0)
	, failed(false)
{
    handle->ref();
}

AsyncLoader::AsyncLoaderItem::~AsyncLoaderItem()
{
    handle->deref();
    ::free(buffer);
}

}
