
#include "config.h"

#include "AXObjectCache.h"
#include "CachedPage.h"
#include "Clipboard.h"
#include "Editor.h"
#include "FrameView.h"
#include "FTPDirectoryDocument.h"
#include "Language.h"
#include "PluginView.h"
#include <float.h>

namespace WebCore 
{
void getSupportedKeySizes(Vector<String>&) {  }
String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String &challengeString, const KURL &url) { return String(); }
float userIdleTime() { return FLT_MAX; } // return an arbitrarily high userIdleTime so that releasing pages from the page cache isn't postponed

}

