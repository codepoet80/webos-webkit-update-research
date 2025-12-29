

#include "config.h"
#include "JSPalmServiceBridgeConstructor.h"

#include "JSPalmServiceBridge.h"
#include "ScriptExecutionContext.h"
#include "PalmServiceBridge.h"

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSPalmServiceBridgeConstructor);

const ClassInfo JSPalmServiceBridgeConstructor::s_info = { "PalmServiceBridgeConstructor", 0, 0, 0 };

JSPalmServiceBridgeConstructor::JSPalmServiceBridgeConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMObject(JSPalmServiceBridgeConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
    , m_globalObject(globalObject)
{
    putDirect(exec->propertyNames().prototype, JSPalmServiceBridgePrototype::self(exec, exec->lexicalGlobalObject()), None);
}

ScriptExecutionContext* JSPalmServiceBridgeConstructor::scriptExecutionContext() const
{
    return m_globalObject->scriptExecutionContext();
}

static JSObject* constructPalmServiceBridge(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    ScriptExecutionContext* context = static_cast<JSPalmServiceBridgeConstructor*>(constructor)->scriptExecutionContext();
    if (!context)
        return throwError(exec, ReferenceError, "PalmServiceBridge constructor associated document is unavailable");

    RefPtr<PalmServiceBridge> palmServiceBridge = PalmServiceBridge::create(context);
    return CREATE_DOM_OBJECT_WRAPPER(exec, PalmServiceBridge, palmServiceBridge.get());
}

ConstructType JSPalmServiceBridgeConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructPalmServiceBridge;
    return ConstructTypeHost;
}

void JSPalmServiceBridgeConstructor::mark()
{
    DOMObject::mark();
    if (!m_globalObject->marked())
        m_globalObject->mark();
}

} // namespace WebCore
