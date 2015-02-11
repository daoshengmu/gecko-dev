
#include "mozilla/dom/HelloPluginRequestChild.h"

namespace mozilla {
namespace dom {

MOZ_IMPLICIT HelloPluginRequestChild::HelloPluginRequestChild()
{
    MOZ_COUNT_CTOR(HelloPluginRequestChild);
}

MOZ_IMPLICIT HelloPluginRequestChild::~HelloPluginRequestChild()
{
    MOZ_COUNT_DTOR(HelloPluginRequestChild);
}

} // namespace dom
} // namespace mozilla
