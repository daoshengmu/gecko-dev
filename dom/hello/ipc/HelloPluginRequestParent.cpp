
#include "mozilla/dom/HelloPluginRequestParent.h"

namespace mozilla {
namespace dom {

void
HelloPluginRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

MOZ_IMPLICIT HelloPluginRequestParent::HelloPluginRequestParent()
{
    MOZ_COUNT_CTOR(HelloPluginRequestParent);
}

MOZ_IMPLICIT HelloPluginRequestParent::~HelloPluginRequestParent()
{
    MOZ_COUNT_DTOR(HelloPluginRequestParent);
}

} // namespace dom
} // namespace mozilla
