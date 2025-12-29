#include "config.h"
#include "CompositeEditCommand.h"
#include "DeleteFromTextNodeCommand.h"
#include "InsertIntoTextNodeCommand.h"
#include "Text.h"
#include <glib.h>

using namespace std;

namespace WebCore {

void CompositeEditCommand::replaceTextInNode(PassRefPtr<Text> node, unsigned offset, unsigned count, const String& replacementText, bool fixingWhiteSpace)
{
	if (fixingWhiteSpace) {
		ASSERT(count == replacementText.length());
		if (count == replacementText.length())
			document()->markers()->updatingMarkersEnabled(false);
		else
			g_warning("CompositeEditCommand::replaceTextInNode fixed white space '%s' is not the same length with original.", replacementText.utf8().data());
	}

    applyCommandToComposite(DeleteFromTextNodeCommand::create(node.get(), offset, count));
    applyCommandToComposite(InsertIntoTextNodeCommand::create(node, offset, replacementText));

    if (fixingWhiteSpace)
    	document()->markers()->updatingMarkersEnabled(true);
}


}
