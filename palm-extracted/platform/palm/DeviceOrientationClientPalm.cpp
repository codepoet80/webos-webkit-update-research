/**
 *******************************************************************************
 *
 * Copyright (c) 2011 Hewlett-Packard Development Company, L.P.
 * All rights reserved.
 *
 * Created on: Oct 27, 2011
 *
 *******************************************************************************
 */
#include "config.h"
#include "DeviceOrientationClientPalm.h"

#include "DeviceOrientationController.h"
#include "DeviceOrientationProviderPalm.h"
#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "palmwebpage.h"
#include "palmwebview.h"
#include "webkitpalmsettings.h"

#include <webkitstats.h>
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

DeviceOrientationClientPalm::DeviceOrientationClientPalm(Palm::WebPage* page)
    : m_WebPage(page)
    , m_controller(0)
    , m_provider(new DeviceOrientationProviderPalm(page))
{
}

DeviceOrientationClientPalm::~DeviceOrientationClientPalm()
{
    delete m_provider;
}

void DeviceOrientationClientPalm::setController(DeviceOrientationController* controller)
{
    m_controller = controller;
    if (m_provider)
        m_provider->setController(controller);
}

void DeviceOrientationClientPalm::startUpdating()
{
    if (m_provider)
        m_provider->start();
}

void DeviceOrientationClientPalm::stopUpdating()
{
    if (m_provider)
        m_provider->stop();
}

DeviceOrientation* DeviceOrientationClientPalm::lastOrientation() const
{
    if (m_provider)
        return m_provider->orientation();

    return 0;
}

void DeviceOrientationClientPalm::deviceOrientationControllerDestroyed()
{
    if (m_provider)
        m_provider->setController(0);

    delete this;
}

} // namespace WebCore
