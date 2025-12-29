
#include "config.h"
#include "V8DOMWindow.h"

#include "V8Binding.h"
#include "V8BindingDOMWindow.h"
#include "V8BindingMacros.h"
#include "V8BindingState.h"
#include "V8CustomEventListener.h"
#include "V8Proxy.h"
#include "V8Utilities.h"

#include "Base64.h"
#include "ExceptionCode.h"
#include "DOMTimer.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "KURL.h"
#include "Settings.h"
#include "CString.h"
#include "LunaResources.h"
#include "SelectionController.h"


namespace WebCore {
	


v8::Handle<v8::Value> V8DOMWindow::caretRectCallback( const v8::Arguments& args )
{
    INC_STATS("DOM.DOMWindow.caretRect()");
    DOMWindow* imp = V8DOMWindow::toNative(args.Holder());

    if (!V8BindingSecurity::canAccessFrame(V8BindingState::Only(), imp->frame(), true))
        return v8::Undefined();

    ASSERT(imp->frame());
    SelectionController* selection = imp->frame()->selection();
    IntRect caretRect;
    if( selection->isFocusedAndActive() && selection->isCaretOrRange() ) {
        caretRect = selection->absoluteCaretBounds();
        //adjust the co-ordinates to unscalled values as expected by framework
        float zoomFactor = imp->zoomFactor();
        if(zoomFactor != 0.0 && zoomFactor != 1.0)
            caretRect.scale((float)1.0 / zoomFactor);
    }

    v8::Handle<v8::Object> rectObject = v8::Object::New();
    rectObject->Set( v8::String::New("x"), v8::Number::New( caretRect.x() ) );
    rectObject->Set( v8::String::New("y"), v8::Number::New( caretRect.y() ) );
    rectObject->Set( v8::String::New("width"), v8::Number::New( caretRect.width() ) );
    rectObject->Set( v8::String::New("height"), v8::Number::New( caretRect.height() ) );
    return rectObject;
}

extern v8::Handle<v8::Value> WindowSetTimeoutImpl(const v8::Arguments& args, bool singleShot);

v8::Handle<v8::Value> V8DOMWindow::webOSSetTimeoutCallback( const v8::Arguments& args )
{
    INC_STATS("DOM.DOMWindow.WebOSSetTimeout()");
    DOMTimer::setMinTimerInterval(0.0f);
    v8::Handle<v8::Value> returnValue = WindowSetTimeoutImpl(args, true);
    DOMTimer::setMinTimerInterval(0.01f);
    return returnValue;
}


} // namespace WebCore
