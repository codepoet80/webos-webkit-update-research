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

#ifndef DeviceMotionClientPalm_h
#define DeviceMotionClientPalm_h

#include "DeviceMotionClient.h"
#include "DeviceMotionData.h"

#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "palmwebpage.h"
#include "palmwebview.h"

namespace WebCore {

class DeviceMotionProviderPalm;

class DeviceMotionClientPalm : public DeviceMotionClient {
public:
    DeviceMotionClientPalm(Palm::WebPage* page);
    virtual ~DeviceMotionClientPalm();

    virtual void setController(DeviceMotionController*);
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual DeviceMotionData* currentDeviceMotion() const;
    virtual void deviceMotionControllerDestroyed();

private:
    Palm::WebPage* m_WebPage;
    DeviceMotionController* m_controller;
    DeviceMotionProviderPalm* m_provider;
};

} // namespece WebCore



#endif /* DeviceMotionClientPalm_h */
