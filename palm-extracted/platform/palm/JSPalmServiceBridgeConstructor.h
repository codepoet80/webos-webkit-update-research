
#ifndef JSPalmServiceBridgeConstructor_h
#define JSPalmServiceBridgeConstructor_h

#include "JSDOMBinding.h"

namespace WebCore {

class JSPalmServiceBridgeConstructor : public DOMObject {
public:
    JSPalmServiceBridgeConstructor(JSC::ExecState*, JSDOMGlobalObject*);
    ScriptExecutionContext* scriptExecutionContext() const;
    static const JSC::ClassInfo s_info;

    virtual void mark();
private:
    virtual JSC::ConstructType getConstructData(JSC::ConstructData&);
    virtual const JSC::ClassInfo* classInfo() const { return &s_info; }

    JSDOMGlobalObject* m_globalObject;
};

} // namespace WebCore

#endif // JSPalmServiceBridgeConstructor_h
