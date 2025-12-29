/* ============================================================
 * Date  : 2009-02-08
 * Copyright 2009 Palm, Inc. All rights reserved.
 * ============================================================ */

#include "config.h"
#include "palmmemstats.h"

#include <stdio.h>
#include <glib.h>

#include <list>
#include <vector>
#include <pbnjson.hpp>
#include <sstream>

#include "palmwebglobal.h"
#include "palmwebpage.h"
#include "palmwebpageclient.h"
#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "webkitpalmsettings.h"
#include "palmwebpageprivate.h"
#include "HTMLAllCollection.h"
#include "PalmServiceBridge.h"

#include "Document.h"
#include "Frame.h"
#include "Page.h"
#include "CString.h"

#if USE(V8)
#include <v8.h>
#endif

namespace WebCore
{
extern std::vector<Document*> sLiveDocumentList;
}

namespace Palm
{

static GHashTable* s_entryHashTable = 0;

//static DBusConnection* s_dbusConnection = 0;
static const char* s_connectionName = "com.palm.luna.memstats";
static const char* s_connectionPath = "/com/palm/luna/memstats";
static const char* s_reportSignalName = "stats";
static int s_periodicReportIntervalSeconds = 1;
static bool s_enableMemoryStats = false;

struct MemStatsEntry {
	char* name;
	int size;
};

static inline MemStatsEntry* findEntry(const char* category)
{
	MemStatsEntry* e = (MemStatsEntry*) g_hash_table_lookup(s_entryHashTable, category);
	if (!e) {
		e = new MemStatsEntry;
		e->name = strdup(category);
		e->size = 0;

		g_hash_table_insert(s_entryHashTable, e->name, e);
	}

	return e;
}

static void wakeupFunction(void* data)
{
	GMainContext* ctxt = g_main_loop_get_context(WebGlobal::mainLoop());
	g_main_context_wakeup(ctxt);
}

static int s_messageBufferIndex = 0;
static const int kMessageBufferSize = 1024;
static char* s_messageBuffer = 0;

static void hashEntryFunction(gpointer key, gpointer value, gpointer userData)
{
	MemStatsEntry* e = (MemStatsEntry*) value;

	s_messageBufferIndex += sprintf(s_messageBuffer + s_messageBufferIndex, "%s;%d;",
									e->name, e->size);
}

static gboolean periodicReportFunction(void* data)
{
#if 0	
	DBusMessage* signal = dbus_message_new_signal(s_connectionPath,
												  s_connectionName,
												  s_reportSignalName);

	if (G_UNLIKELY(s_messageBuffer == 0))
		s_messageBuffer = (char*) malloc(kMessageBufferSize);
	
	s_messageBufferIndex = 0;	
	memset(s_messageBuffer, 0, kMessageBufferSize);

	g_hash_table_foreach(s_entryHashTable, hashEntryFunction, NULL);

	dbus_message_append_args(signal,
							 DBUS_TYPE_STRING, &s_messageBuffer,
							 DBUS_TYPE_INVALID);
	
	if (!dbus_connection_send(s_dbusConnection, signal, NULL)) {
		fprintf(stderr, "MemStats: Failed to send stats signal\n");
	}

	dbus_message_unref(signal);
#endif

	return TRUE;
}

void MemStats::init()
{
#if 0	
	static bool initialized = false;
	if (G_LIKELY(initialized))
		return;

	initialized = true;

	s_enableMemoryStats = PalmBrowserSettings()->enableMemoryStats;
	if (!s_enableMemoryStats) {
		return;	
	}

	s_entryHashTable = g_hash_table_new(g_str_hash, g_str_equal);	

	DBusError err;
	dbus_error_init(&err);
	
	s_dbusConnection = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
	if (!s_dbusConnection) {
		if (dbus_error_is_set(&err)) {
			fprintf(stderr, "MemStats: %s\n", err.message);
			dbus_error_free(&err);
		}
		else {
			fprintf(stderr, "MemStats: Failed to register with bus\n");
		}
		return;
	}

	if (dbus_bus_request_name(s_dbusConnection, s_connectionName,
							  DBUS_NAME_FLAG_DO_NOT_QUEUE, &err)
		!= DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {

		if (dbus_error_is_set(&err)) {
			fprintf(stderr, "MemStats: %s\n", err.message);
			dbus_error_free(&err);
		}
		else {
			fprintf(stderr, "MemStats: Failed to request name %s with bus\n",
				   s_connectionName);
		}
		return;
	}


	GMainContext* ctxt = g_main_loop_get_context(WebGlobal::mainLoop());
	
/*
	Don't need this currently
	
	dbus_connection_set_wakeup_main_function(s_dbusConnection, wakeupFunction,
											 0, 0);	
	g_dbus_setup_connection(s_dbusConnection, ctxt);
*/

	GSource* timeoutSrc = g_timeout_source_new_seconds(s_periodicReportIntervalSeconds);
	g_source_set_callback(timeoutSrc, periodicReportFunction, NULL, NULL);
	g_source_attach(timeoutSrc, ctxt);
	g_source_unref(timeoutSrc);
#endif
}

void MemStats::inc(const char* category, int size)
{
	init();

	if (!s_enableMemoryStats)
		return;	
	
	MemStatsEntry* entry = findEntry(category);
	entry->size += size;
}

void MemStats::dec(const char* category, int size)
{
	init();

	if (!s_enableMemoryStats)
		return;	
	
	MemStatsEntry* entry = findEntry(category);
	entry->size -= size;
}

void MemStats::set(const char* category, int size)
{
	init();

	if (!s_enableMemoryStats)
		return;	
	
	MemStatsEntry* entry = findEntry(category);
	entry->size = size;
}

void MemStats::dumpToConsole()
{
	if (!s_enableMemoryStats)
		return;	
    
}

/*

 {
        "documents": [
                {
                        "url": "<...>",
                        "nodes":3948
                },
                {
                        "url": "<...>",
                        "nodes":3948
                },

                ...

                ],

       "counters": {
                "heapCapacity": 39488392,
                "heapSize": 238483883,
       "foo":485,
       }
}
Reasoning:
- the "documents" array contains frames of documents, so we can add more fields for 
  each document as we feel necessary. We'll make a requirement there is at least a "nodes" and "url".
- the "counters" frame is a misc. "throw in anything you want." This a global 
  resource counter for webkit. You could put gfx surfaces in here, fonts, etc., doesn't 
  matter. We'll agree on the naming for a few of these (e.g., javascript vm items)


  I'll make sure the last char in the JSON string returned form webkit is the "}" so 
  LunaSysMgr can insert anything else it wants directly into the root frame w/o complicated 
  parsing. Alternatively, LunaSysMgr could take this whole frame and stick it in it's own frame...
*/

static std::string jsonToString(pbnjson::JValue& reply, const char* schema = "{}")
{
    pbnjson::JGenerator serializer(NULL);   // our schema that we will be using does not have any external references
    std::string serialized;
    pbnjson::JSchemaFragment responseSchema(schema);
    if (!serializer.toString(reply, responseSchema, serialized)) {
            g_critical("jsonToString: failed to generate json reply");
            return "{\"returnValue\":false,\"errorText\":\"error: Failed to generate a valid json reply...\"}";
    }
    return serialized;
}
static std::string stripJsonObjectOuterBraces(const std::string& json_object_string)
{
	size_t pos = json_object_string.find("{") + 1;
	size_t rpos = json_object_string.rfind("}");
	size_t n = rpos - pos;
	std::string ss_without_outer_braces = json_object_string.substr(pos,n);

	return ss_without_outer_braces;
}

bool MemStats::getJSON( std::multimap<std::string,std::string>& outDocMapJSON, std::string& outCountersJSON )
{
	// Documents
	{
		for( std::vector<WebCore::Document*>::iterator it=WebCore::sLiveDocumentList.begin(); it != WebCore::sLiveDocumentList.end(); ++it ) 
		{
			WebCore::Document* doc = *it;
			std::string appId;
			pbnjson::JValue docResponse = pbnjson::Object();

			// Does this document have an AppId? If so --
			if( doc->page() )
			{
				if( WebFrame* frame = kit( doc->frame() ) )
				{
					if( frame->client()->getIdentifier() )
						appId = frame->client()->getIdentifier();
				}
			}
			
			docResponse.put("appId", std::string( appId.c_str() ));
            docResponse.put("refCount", (int64_t)doc->refCount());
			docResponse.put("url", doc->url().string().utf8().data());
			docResponse.put("openServiceHandles", WebCore::PalmServiceBridge::numHandlesForUrl( doc->url().string().utf8().data() ));

			if( ::PalmBrowserSettings()->debugServiceHandles )
			{
				pbnjson::JValue j_serviceHandles = pbnjson::Array();

				std::list<WebCore::PalmServiceBridge*> handles;
				WebCore::PalmServiceBridge::handlesForUrl( doc->url().string().utf8().data(), handles );
				for( std::list<WebCore::PalmServiceBridge*>::iterator hit=handles.begin(); hit != handles.end(); ++hit ) 
				{
					pbnjson::JValue j_handle = pbnjson::Object();
					j_handle.put("uri", (*hit)->uri() ? (*hit)->uri()->utf8().data() : "");
					std::stringstream payload_value_ss;
					payload_value_ss << "callbacks=" << (*hit)->returnCount() << " ";
					if ( (*hit)->payload() ) {
						payload_value_ss << (*hit)->payload()->utf8().data();
					}
					j_handle.put("payload", payload_value_ss.str());

					j_serviceHandles.append(j_handle);
				}

				docResponse.put("serviceHandles", j_serviceHandles);
			}
			// nodes (in + disconnected w/EventListeners)
			WTF::RefPtr<WebCore::HTMLAllCollection> allNodes = doc->all();
			docResponse.put("nodes", (int64_t) (allNodes->length()));
			
			std::string ss = jsonToString(docResponse, "{\"type\":\"object\"}");

			// FIXME: Clients of this MemStats::getJSON should expect JSON objects (i.e. starts with '{' and ends with '}')
			// Unfortunately, the clients, like LunaSysMgr's WebAppManager.cpp wrap the result in their own set of curly braces.
			// To keep the expected behavior the same for these clients, we are stripping off our set of braces
			// When the clients are fixed, this ugly hack can be removed.

			std::string ss_without_outer_braces = stripJsonObjectOuterBraces(ss);

			outDocMapJSON.insert( std::pair<std::string, std::string>(appId,ss_without_outer_braces));
		}
	}
	
	// Counters	
	{
		pbnjson::JValue counterResponse = pbnjson::Object();
#if USE(V8)
        v8::HeapStatistics stats;
        v8::V8::GetHeapStatistics(&stats);

        pbnjson::JValue j_jsHeap = pbnjson::Object();
        {
        	j_jsHeap.put("used", (int64_t) stats.used_heap_size());
        	j_jsHeap.put("available", (int64_t) stats.available_heap_size());
        	j_jsHeap.put("capacity", (int64_t) stats.heap_capacity());
        	j_jsHeap.put("committed", (int64_t) stats.total_heap_size());

        	j_jsHeap.put("new-used", (int64_t) stats.used_new_space_size());
        	j_jsHeap.put("new-avail", (int64_t) stats.available_new_space_size());
        	j_jsHeap.put("new-waste", (int64_t) stats.wasted_new_space_size());
        	j_jsHeap.put("new-capacity", (int64_t) stats.new_space_capacity());

        	j_jsHeap.put("old_pointer-used", (int64_t) stats.used_old_pointer_space_size());
        	j_jsHeap.put("old_pointer-avail", (int64_t) stats.available_old_pointer_space_size());
        	j_jsHeap.put("old_pointer-waste", (int64_t) stats.wasted_old_pointer_space_size());
        	j_jsHeap.put("old_pointer-capacity", (int64_t) stats.old_pointer_space_capacity());

        	j_jsHeap.put("old_data-used", (int64_t) stats.used_old_data_space_size());
        	j_jsHeap.put("old_data-avail", (int64_t) stats.available_old_data_space_size());
        	j_jsHeap.put("old_data-waste", (int64_t) stats.wasted_old_data_space_size());
        	j_jsHeap.put("old_data-capacity", (int64_t) stats.old_data_space_capacity());

        	j_jsHeap.put("code-used", (int64_t) stats.used_code_space_size());
        	j_jsHeap.put("code-avail", (int64_t) stats.available_code_space_size());
        	j_jsHeap.put("code-waste", (int64_t) stats.wasted_code_space_size());
        	j_jsHeap.put("code-capacity", (int64_t) stats.code_space_capacity());

        	j_jsHeap.put("map-used", (int64_t) stats.used_map_space_size());
        	j_jsHeap.put("map-avail", (int64_t) stats.available_map_space_size());
        	j_jsHeap.put("map-waste", (int64_t) stats.wasted_map_space_size());
        	j_jsHeap.put("map-capacity", (int64_t) stats.map_space_capacity());

        	j_jsHeap.put("cell-used", (int64_t) stats.used_cell_space_size());
        	j_jsHeap.put("cell-avail", (int64_t) stats.available_cell_space_size());
        	j_jsHeap.put("cell-waste", (int64_t) stats.wasted_cell_space_size());
        	j_jsHeap.put("cell-capacity", (int64_t) stats.cell_space_capacity());

        	j_jsHeap.put("lo-used", (int64_t) stats.used_lo_space_size());
        	j_jsHeap.put("lo-avail", (int64_t) stats.available_lo_space_size());
        	j_jsHeap.put("lo-waste", (int64_t) stats.wasted_lo_space_size());
        	j_jsHeap.put("lo-capacity", (int64_t) stats.lo_space_capacity());

        	j_jsHeap.put("gc-count", (int64_t) stats.gc_count());
        	j_jsHeap.put("scavenge-count", (int64_t) stats.scavenge_count());
        	j_jsHeap.put("mark-sweep-count", (int64_t) stats.mark_sweep_count());
        	j_jsHeap.put("mark-compact-count", (int64_t) stats.mark_compact_count());

        }
        counterResponse.put("jsHeap", j_jsHeap);
#else
        counterResponse.put("jsHeapCapacity", 0);
        counterResponse.put("jsHeapSize", 0);
#endif
		
		// ... 
		std::string ss = jsonToString(counterResponse, "{\"type\":\"object\"}");
		std::string ss_without_outer_braces = stripJsonObjectOuterBraces(ss);

		outCountersJSON = ss_without_outer_braces;
	}
	
	return true;
}


}

