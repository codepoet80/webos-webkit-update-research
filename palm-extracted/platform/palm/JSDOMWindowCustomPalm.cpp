

#include "config.h"
#include "JSDOMWindowCustom.h"

#include "AtomicString.h"
#include "Base64.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "History.h"
#include "JSAudioConstructor.h"
#include "JSDOMWindowShell.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "JSHTMLCollection.h"
#include "JSHistory.h"
#include "JSImageConstructor.h"
#include "JSLocation.h"
#include "JSMessageChannelConstructor.h"
#include "JSMessagePort.h"
#include "JSOptionConstructor.h"
#include "JSWebKitCSSMatrixConstructor.h"
#include "JSWebKitPointConstructor.h"
#include "JSWorkerConstructor.h"
#include "JSXMLHttpRequestConstructor.h"
#include "JSXSLTProcessorConstructor.h"
#include "Location.h"
#include "MediaPlayer.h"
#include "MessagePort.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "RegisteredEventListener.h"
#include "ScheduledAction.h"
#include "ScriptController.h"
#include "Settings.h"
#include "WindowFeatures.h"
#include <runtime/JSObject.h>
#include <runtime/PrototypeFunction.h>

#include "LunaResources.h"
#include "Interpreter.h"
#include "UString.h"
#include "CString.h"

#include "JSPalmServiceBridgeConstructor.h"
#include "JSPalmServiceBridge.h"

using namespace JSC;

namespace WebCore {

JSValue JSDOMWindow::palmServiceBridge(ExecState* exec) const
{
    return getDOMConstructor<JSPalmServiceBridgeConstructor>(exec, this);
}

JSValue JSDOMWindow::palmGetResource(ExecState* exec, const ArgList& args)
{
	if( args.size() < 1 )
		return jsUndefined();
	
	
	UString filename = args.at(0).toString(exec);
	String result;
	bool showError = true;
	
	if( args.size() >= 2 )
		showError = !args.at(1).toBoolean(exec);
	
	// I need to know the sourceURL of the document making this request. This is *not* necessarily the url
	// of the active frame. We need to pull it from the ExecState.
	
	int signedLineNumber;
    intptr_t sourceID;
    UString urlString;
    JSValue function;
	exec->interpreter()->retrieveLastCaller( exec, signedLineNumber, sourceID, urlString, function );
	if( urlString.size() ) 
	{
		String base( urlString.data(), urlString.size() );
		String path( filename.data(), filename.size() );
		KURL mergedUrl( KURL(base), path );
		
		//printf(" palmGetResource( %s + %s --> \n                    %s\n", 
		//	base.utf8().data(), base.utf8().data(), mergedUrl.string().utf8().data() );
		
		if( !LunaResources::instance()->getResource( mergedUrl.path().utf8().data(), result ) ) {
			//if( showError ) 
				fprintf(stderr," ***************** palmGetResource( %s ) -- file not found\n", path.utf8().data() );
			return jsNull();		
		}
	}
	else
	{
		String path( filename.data(), filename.size() );
		if( !LunaResources::instance()->getResource( path, result ) ) {
			if( showError ) 
				fprintf(stderr," ***************** palmGetResource( %s ) -- file not found\n", path.utf8().data() );
			return jsNull();		
		}
	}
	
	return jsString( exec, result );
}

}


