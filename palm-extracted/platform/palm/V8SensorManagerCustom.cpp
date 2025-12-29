/**
 *******************************************************************************
 *
 * Copyright (c) 2011 Hewlett-Packard Development Company, L.P.
 * All rights reserved.
 *
 * Created on: Oct 5, 2011
 *
 *******************************************************************************
 */


#include "config.h"

#if ENABLE(WEBOS_SENSORS)

#include "V8SensorManager.h"

#include "Base64.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "Sensor.h"
#include "SensorManager.h"
#include "Settings.h"
#include "V8Binding.h"
#include "V8CustomEventListener.h"
#include "V8Proxy.h"
#include "V8Sensor.h"
#include "V8Utilities.h"

namespace WebCore {

v8::Handle<v8::Value> V8SensorManager::createSensorCallback(const v8::Arguments& args)
{
    INC_STATS(L"DOM.SensorManager.createSensor()");
    SensorManager* sensorManagerObj = V8SensorManager::toNative(args.Holder());

    if (sensorManagerObj) {
        ExceptionCode ec = 0;
        String sensorName = toWebCoreString(args[0]);

        RefPtr<Sensor> sensor = sensorManagerObj->createSensor(sensorName, ec);

        if (ec)
            return throwError(ec);

        // else
        sensor->ref();
        return toV8(sensor.get());

    }

    return throwError(INVALID_STATE_ERR);
}

}

#endif // #if ENABLE(WEBOS_SENSORS)
