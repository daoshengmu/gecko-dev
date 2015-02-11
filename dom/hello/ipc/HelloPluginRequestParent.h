
#ifndef mozilla_dom_HelloPluginRequestParent_h
#define mozilla_dom_HelloPluginRequestParent_h

#include "mozilla/dom/PHelloPluginRequestParent.h"

namespace mozilla {
namespace dom {
class HelloPluginRequestParent : public PHelloPluginRequestParent
{
public:
    MOZ_IMPLICIT HelloPluginRequestParent();
    virtual ~HelloPluginRequestParent();

protected:
    virtual void
    ActorDestroy(ActorDestroyReason aWhy);

};
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HelloPluginRequestParent_h
