/* ============================================================
 * Date  : 2010-01-01
 * Copyright 2010 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef ASYNCLOADER_H
#define ASYNCLOADER_H

#include "PlatformString.h"
#include "Threading.h"

#include <list>
#include <glib.h>

namespace WebCore
{

class ResourceHandle;

class AsyncLoader
{
public:

	static AsyncLoader* instance();

	bool load(const String& path, ResourceHandle* handle);
	void cancel(ResourceHandle* handle);

private:

	struct AsyncLoaderItem {

		AsyncLoaderItem(const String& path, const String& filePath, ResourceHandle* handle);
		~AsyncLoaderItem();

		String path;
		String filePath;
		ResourceHandle* handle;
		char* buffer;
		long bufferSize;
		bool failed;
	};

private:
	
	AsyncLoader();
	~AsyncLoader();

	static gpointer threadFunction(gpointer arg);
	static gboolean threadCallbackOnMainThread(GIOChannel* channel, GIOCondition condition, gpointer arg);
	void load(AsyncLoaderItem* item);
	void oneLoadFinished();
	void wakeupMainThread();
	void wakeupChildThread();

private:

	std::list<AsyncLoaderItem*> m_jobQueue;
	std::list<AsyncLoaderItem*> m_finishedQueue;

	Mutex m_mutex;
	GThread* m_thread;
	int m_mainToThreadPipeFd[2];
	int m_threadToMainPipeFd[2];
	GIOChannel* m_ioChannel;
	GSource* m_ioSource;
};

}

#endif /* ASYNCLOADER_H */
