
#include "config.h"
#include "PlatformBridge.h"
#include "webkitpalmsettings.h"
#include "PluginView.h"

namespace WebCore
{

bool PlatformBridge::popupsAllowed( NPP )
{ 
	return PalmBrowserSettings()->java_script_can_open_windows_automatically; 
}

NPObject* PlatformBridge::pluginScriptableObject( Widget* widget )
{
	PluginView* view = static_cast<PluginView*>(widget);
	return view->bindingInstance();
}

}


