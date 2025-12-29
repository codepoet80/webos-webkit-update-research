#include "config.h"
#include "FrameLoader.h"

#include "FrameLoaderClient.h"
#include "ResourceLoader.h"

namespace WebCore {

void FrameLoader::didReceiveSSLCertValidationFail(ResourceLoader* loader,Palm::SSLValidationInfo& sslInfo)
{
    m_client->dispatchDidReceiveSSLCertValidationFail(loader->documentLoader(),sslInfo);
}

}
