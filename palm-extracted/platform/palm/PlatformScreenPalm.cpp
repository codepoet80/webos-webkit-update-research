
#include "config.h"
#include "PlatformScreen.h"

#include "NotImplemented.h"
#include "Widget.h"

#include "palmwebview.h"
#include "palmwebviewclient.h"

#include <stdio.h>
#include <wtf/Assertions.h>

namespace WebCore {

int screenDepth(Widget* widget) 
{
	// notImplemented();
    return 32;
}

int screenDepthPerComponent(Widget*)
{
    //notImplemented();
    return 8;
}

bool screenIsMonochrome(Widget*) 
{ 
    notImplemented(); 
    return false; 
}

// We want these to return device pixels, since that's 
// what other ports seem to do. This means, we do not
// want to return scaled anything.
FloatRect screenRect(Widget* widget) 
{
	if (!widget)
		return FloatRect();
	
    Palm::WebView* view = widget->containingWindow();
	if (!view)
		return FloatRect();

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient)
		return FloatRect();
	
    int winWidth, winHeight;
    viewClient->getScreenSize(winWidth, winHeight);	
		
	return FloatRect( 0, 0, winWidth, winHeight );
}

// We want these to return device pixels, since that's 
// what other ports seem to do. This means, we do not
// want to return scaled anything.
FloatRect screenAvailableRect(Widget* widget) 
{ 
    return screenRect(widget);
}

} // namespace WebCore
