

#include "config.h"
#include "NetworkStateNotifier.h"

namespace WebCore {

void NetworkStateNotifier::updateState()
{
}

NetworkStateNotifier::NetworkStateNotifier()
    : m_isOnLine(true)
    , m_networkStateChangedFunction(0)
{
}

} // namespace WebCore
