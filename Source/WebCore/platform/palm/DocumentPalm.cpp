

#include "config.h"
#include "Document.h"
#include "Page.h"
#include "Chrome.h"
#include "ChromeClient.h"

namespace WebCore {


String Document::toString() const
{
	abort(); // GREG TODO
/*    String result;

    for (Node *child = firstChild(); child != NULL; child = child->nextSibling()) {
        result += child->toString();
    }

    return result;
    */
    return String();
}

void Document::registerTouchListener()
{
    addListenerType(TOUCH_LISTENER);
    if (Page* page = this->page())
        page->chrome()->client()->needTouchEvents(true);
}

void Document::unregisterTouchListener()
{
    if (Page* page = this->page())
        page->chrome()->client()->needTouchEvents(false);
}


} // WebCore

