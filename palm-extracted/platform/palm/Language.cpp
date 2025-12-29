
#include "config.h"
#include "Language.h"

#include "PlatformString.h"
#include "palmwebglobal.h"

namespace WebCore {

String defaultLanguage()
{
    return Palm::WebGlobal::getLocale();
}

}
