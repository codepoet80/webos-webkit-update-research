
// Copyright (C) 2010 HP Inc.  All rights reserved.
 
#include "config.h"
#include "SocketStreamHandle.h"

#include "KURL.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "SocketStreamHandleClient.h"
#include "palmwebglobal.h"
#include "CString.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <glib.h>
#include <netdb.h>

// Define this to get more verbose logging.
//#define VERBOSE_MESSAGES

#ifdef VERBOSE_MESSAGES
#define SHTRACE(...) {printf("SocketStreamHandle: " __VA_ARGS__); fflush(stdout);}
#else
#define SHTRACE(...) (void)0
#endif


namespace WebCore {
	
class SocketStreamHandlePrivate
{
public:
	SocketStreamHandlePrivate(SocketStreamHandle* t);
	~SocketStreamHandlePrivate();
	
	bool connect(const KURL& url);
	void disconnect();
	
	SocketStreamHandle* m_streamHandle;
	
	int m_socket;
	GIOChannel* m_channel;
	GSource* m_dataSource;
	GSource* m_hupSource;
	static gboolean socketDataCallback(GIOChannel* /*channel*/, GIOCondition /*condition*/, gpointer arg);
	static gboolean socketDisconnect(GIOChannel* /*channel*/, GIOCondition /* condition*/, gpointer arg);

	static void connectCallback( void* );
};


static bool IsAddressNumeric( const char* inaddr )
{
	while( *inaddr )
	{
		if( !isdigit(*inaddr) && (*inaddr) != '.' )
			return false;
		inaddr++;
	}
	return true;
}

SocketStreamHandlePrivate::SocketStreamHandlePrivate(SocketStreamHandle* t)
	: m_streamHandle( t )
	, m_socket(-1)
	, m_channel(0)
{
}

bool SocketStreamHandlePrivate::connect( const KURL& url)
{
	m_socket = ::socket( PF_INET, SOCK_STREAM, 0);
	
	SHTRACE("%p SocketStreamHandlePrivate::connect %s protocol=%s host=%s port=%d\n", this, 
		url.string().utf8().data(),
		url.protocol().utf8().data(), url.host().utf8().data(), url.port() );
	
	struct hostent * host ;
	struct sockaddr_in internetAddr;
	
	 
	bool isSecure = url.protocolIs("wss");
	unsigned int port = url.hasPort() ? url.port() : (isSecure ? 443 : 80);
	char* hostAddr = strdup( url.host().utf8().data() );
	
	SHTRACE("socketstream m_socket=%d %s:%d\n", m_socket, hostAddr, port );
	
	if( IsAddressNumeric(hostAddr) )
	{
		memset( &internetAddr, 0, sizeof(internetAddr) );
		internetAddr.sin_addr.s_addr = inet_addr(hostAddr);
		internetAddr.sin_family=AF_INET;
		internetAddr.sin_port=htons(port);
	}
	else
	{
		host = ::gethostbyname( hostAddr );
		if( !host )
		{
			fprintf(stderr,"gethostbyname failed for %s\n", hostAddr);
			free(hostAddr);
			return false;
		}
		
		memcpy( &internetAddr.sin_addr, host->h_addr_list[0], host->h_length );
		internetAddr.sin_family=AF_INET;
		internetAddr.sin_port=htons(port);
	}
	
	if( ::connect( m_socket, (struct sockaddr *)&internetAddr, sizeof(internetAddr)) )
	{
		fprintf(stderr,"Failed to connect() to target.\n");
		free(hostAddr);
		return false;
	}
	
	WTF::callOnMainThread( SocketStreamHandlePrivate::connectCallback, this );
	
	free(hostAddr);
	
	m_channel = g_io_channel_unix_new( m_socket ); 
    (void)g_io_channel_set_encoding(m_channel,0, 0);
	g_io_channel_set_buffered(m_channel, FALSE);

	m_dataSource = g_io_create_watch( m_channel, G_IO_IN );
	m_hupSource = g_io_create_watch( m_channel, (GIOCondition)(G_IO_HUP | G_IO_ERR) );
	
	g_source_set_callback( m_dataSource,  (GSourceFunc)SocketStreamHandlePrivate::socketDataCallback, this, 0 );
	g_source_set_callback( m_hupSource,  (GSourceFunc)SocketStreamHandlePrivate::socketDisconnect, this, 0 );
	
	g_source_attach( m_dataSource, g_main_loop_get_context( Palm::WebGlobal::mainLoop() ) );
	g_source_attach( m_hupSource, g_main_loop_get_context( Palm::WebGlobal::mainLoop() ) );

	g_source_unref( m_dataSource );
	g_source_unref( m_hupSource );
	
	return true;
}

void SocketStreamHandlePrivate::connectCallback( void* user )
{
	SocketStreamHandlePrivate* pThis = (SocketStreamHandlePrivate*)user;
	
    if (pThis->m_streamHandle && pThis->m_streamHandle->client()) {
        pThis->m_streamHandle->m_state = SocketStreamHandleBase::Open;
        pThis->m_streamHandle->client()->didOpen(pThis->m_streamHandle);
    }
}

void SocketStreamHandlePrivate::disconnect()
{
	if( -1 == m_socket )
		return;
	
	SHTRACE("%p SocketStreamHandlePrivate::disconnect\n",this);
	g_source_destroy( m_dataSource );
	g_source_destroy( m_hupSource );
	
	::close( m_socket );
	m_socket = -1;
	
	if (m_streamHandle && m_streamHandle->client()) {
		SocketStreamHandle* handle = m_streamHandle;
        handle->client()->didClose(handle);
        handle->setClient(0);
    }
    
}

gboolean SocketStreamHandlePrivate::socketDataCallback(GIOChannel* /*channel*/, GIOCondition condition, gpointer arg)
{
	SocketStreamHandlePrivate* pThis = (SocketStreamHandlePrivate*)arg;
	
	SHTRACE("%p SocketStreamHandlePrivate::socketDataCallback cond=%08x  socket=%d\n", pThis, condition, pThis->m_socket );
	
	if( -1 == pThis->m_socket )
		return TRUE;
	
    if (pThis->m_streamHandle && pThis->m_streamHandle->client()) {
        const int kReadChunkSize = 2048;
        int readTotal = 0;
        char* data = new char[kReadChunkSize];
        if( data ) {
        	while( 1 ) {
				int sz = ::recv( pThis->m_socket, data, kReadChunkSize, 0 );
				if( !sz )
					break;
				SHTRACE("SocketStreamHandlePrivate::socketDataCallback read %d bytes\n", sz );
				pThis->m_streamHandle->client()->didReceiveData( pThis->m_streamHandle, data, sz );
				readTotal += sz;
				if( sz < kReadChunkSize )
					break;
			}
			
        	delete[] data;
        	
        	if( !readTotal ) {
        		// read nothing so assume socket is dead.
        		pThis->disconnect();
        		return FALSE;
        	}
        }
    }
	
	return TRUE;
}

gboolean SocketStreamHandlePrivate::socketDisconnect(GIOChannel* /*channel*/, GIOCondition /* condition*/, gpointer arg)
{
	SocketStreamHandlePrivate* pThis = (SocketStreamHandlePrivate*)arg;
	SHTRACE("SocketStreamHandlePrivate::socketDisconnect\n");
	
	if (pThis->m_streamHandle && pThis->m_streamHandle->client()) {
		SocketStreamHandle * handle = pThis->m_streamHandle;
		pThis->m_streamHandle =0 ;
		handle->m_state = SocketStreamHandleBase::Closed;
		handle->client()->didClose( handle );
	}
	
	return FALSE;
}

SocketStreamHandlePrivate::~SocketStreamHandlePrivate()
{
	disconnect();
}


SocketStreamHandle::SocketStreamHandle(const KURL& url, SocketStreamHandleClient* client)
    : SocketStreamHandleBase(url, client)
{
    m_p = new SocketStreamHandlePrivate( this ); 
   	m_p->connect(url);
 
}

SocketStreamHandle::~SocketStreamHandle()
{
    LOG(Network, "SocketStreamHandle %p delete", this);
    delete m_p;
    setClient(0);
}

int SocketStreamHandle::platformSend(const char* data, int length)
{
    LOG(Network, "SocketStreamHandle %p platformSend", this);
    int sent =0 ;
    
    if( m_p->m_socket != -1 ) {
    	sent =  (int)::send( m_p->m_socket, data, length, 0 );
    }	
    
   	SHTRACE("sent -> %d/%d bytes\n", sent, length );
    return sent;
}

void SocketStreamHandle::platformClose()
{
    SHTRACE("%p SocketStreamHandle  platformClose\n", this);
    m_p->disconnect();
}

void SocketStreamHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
    notImplemented();
}

void SocketStreamHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCancellation(const AuthenticationChallenge&)
{
    notImplemented();
}

}  // namespace WebCore
