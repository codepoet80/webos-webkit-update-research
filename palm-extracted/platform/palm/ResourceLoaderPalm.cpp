#include "config.h"
#include "ResourceLoader.h"

#include "FrameLoader.h"

namespace WebCore {

void ResourceLoader::didReceiveSSLCertValidationFail(Palm::SSLValidationInfo& sslInfo)
{
    RefPtr<ResourceLoader> protector(this);
    frameLoader()->didReceiveSSLCertValidationFail(this, sslInfo);
}

}
