// Copyright 2009 Palm Inc.

#ifndef ResourceResponse_h
#define ResourceResponse_h

#include "ResourceResponseBase.h"

namespace WebCore {

class ResourceResponse : public ResourceResponseBase {
public:
    ResourceResponse()
          : m_responseFired(false)
    {
		m_httpStatusCode=0;
    }

    ResourceResponse(const KURL& url, const String& mimeType, long long expectedLength, const String& textEncodingName, const String& filename)
        : ResourceResponseBase(url, mimeType, expectedLength, textEncodingName, filename)
          , m_responseFired(false)
    {
		m_httpStatusCode=0;
    }

    void setResponseFired(bool fired) { m_responseFired = fired; }
    bool responseFired() { return m_responseFired; }

private:
    friend class ResourceResponseBase;

    void doUpdateResourceResponse()
    {
    }

    bool m_responseFired;

};

} // namespace WebCore

#endif // ResourceResponse_h
