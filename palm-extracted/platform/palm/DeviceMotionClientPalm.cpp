/**
 *******************************************************************************
 *
 * Copyright (c) 2011 Hewlett-Packard Development Company, L.P.
 * All rights reserved.
 *
 * Created on: Nov. 01, 2011
 *
 *******************************************************************************
 */

#include "config.h"
#include "DeviceMotionClientPalm.h"

#include "DeviceMotionController.h"
#include "DeviceMotionProviderPalm.h"
#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "palmwebpage.h"
#include "palmwebview.h"
#include "webkitpalmsettings.h"

#include <webkitstats.h>
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

DeviceMotionClientPalm::DeviceMotionClientPalm(Palm::WebPage* page)
    : m_WebPage(page)
    , m_controller(0)
    , m_provider(new DeviceMotionProviderPalm(page))
{
}

DeviceMotionClientPalm::~DeviceMotionClientPalm()
{
    delete m_provider;
}

void DeviceMotionClientPalm::setController(DeviceMotionController* controller)
{
    m_controller = controller;
    m_provider->setController(controller);
}

void DeviceMotionClientPalm::startUpdating()
{
    m_provider->start();
}

void DeviceMotionClientPalm::stopUpdating()
{
    m_provider->stop();
}

DeviceMotionData* DeviceMotionClientPalm::currentDeviceMotion() const
{
    if (m_provider)
        return m_provider->currentDeviceMotion();

    return 0;
}

void DeviceMotionClientPalm::deviceMotionControllerDestroyed()
{
    if (m_provider)
        m_provider->setController(0);

    delete this;
}

} // namespace WebCore
