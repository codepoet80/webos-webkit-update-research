/* ============================================================
 * Copyright 2011 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef WEBOSWORDCOMPLETIONCONTROLLER_H
#define WEBOSWORDCOMPLETIONCONTROLLER_H

#include "config.h"
#include "IntPoint.h"

namespace Palm
{

class WebPage;

class WordCompletionController
{
public:
    WordCompletionController(Palm::WebPage* page) : m_page(page), m_handledDown(false) {}

    bool handleMouseDownEvent(const WebCore::IntPoint& viewPt);
    bool handleMouseMoveEvent() { return m_handledDown; }
    bool handleMouseUpEvent() { return (m_handledDown && (m_handledDown = false)); }

    bool mouseCaptured() const { return m_handledDown; }

private:
    Palm::WebPage* m_page;
    bool m_handledDown;
};

}

#endif
