

#include "config.h"
#include "JSPalmServiceBridge.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "JSDOMWindowCustom.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "PalmServiceBridge.h"
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {


void JSPalmServiceBridge::mark()
{
    Base::mark();
	
	markIfNotNull(m_impl->onservicecallback());
}

} // namespace WebCore

