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

#ifndef DeviceOrientationClientPalm_h
#define DeviceOrientationClientPalm_h

#include "DeviceOrientation.h"
#include "DeviceOrientationClient.h"

#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "palmwebpage.h"
#include "palmwebview.h"
#include <glib.h>

using namespace Palm;

namespace WebCore {

class DeviceOrientationProviderPalm;

class DeviceOrientationClientPalm : public DeviceOrientationClient {
public:
    DeviceOrientationClientPalm(Palm::WebPage* page);
    virtual ~DeviceOrientationClientPalm();

    virtual void setController(DeviceOrientationController*);
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual DeviceOrientation* lastOrientation() const;
    virtual void deviceOrientationControllerDestroyed();

private:
    Palm::WebViewClient* getClient();

    Palm::WebPage* m_WebPage;
    DeviceOrientationController* m_controller;
    DeviceOrientationProviderPalm* m_provider;
};

} // namespace WebCore


#endif /* DeviceOrientationClientPalm_h */
