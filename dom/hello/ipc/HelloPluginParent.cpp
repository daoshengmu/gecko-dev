#include "HelloPluginParent.h"
#include "mozilla/dom/HelloPluginRequestParent.h"

namespace mozilla {
namespace dom {

PHelloPluginRequestParent*
HelloPluginParent::AllocPHelloPluginRequestParent(const HelloPluginRequestArgs& requestType)
{
    return 0;
}

bool
HelloPluginParent::DeallocPHelloPluginRequestParent(PHelloPluginRequestParent* aActor)
{
    return false;
}

} // namespace dom
} // namespace mozilla
