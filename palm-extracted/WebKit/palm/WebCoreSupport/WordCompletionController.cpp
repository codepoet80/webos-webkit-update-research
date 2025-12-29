/* ============================================================
 * Copyright 2011 Palm, Inc. All rights reserved.
 * ============================================================ */

#include "WordCompletionController.h"

#include "EditorClientPalm.h"
#include "palmwebpageprivate.h"

namespace Palm
{

bool WordCompletionController::handleMouseDownEvent(const WebCore::IntPoint& viewPt)
{
    m_handledDown = false;

    WebKit::EditorClient* client = static_cast<WebKit::EditorClient*>(core(m_page)->editorClient());
    if (client) {

        m_handledDown = client->pointInWordCompletion(viewPt);
        if (m_handledDown)
            client->commitWordCompletion();
    }
    return m_handledDown;
}

}

