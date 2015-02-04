
#ifndef mozilla_dom_GreetParent_h
#define mozilla_dom_GreetParent_h

#include "mozilla/dom/PGreetParent.h"

//-----------------------------------------------------------------------------
// Skeleton implementation of abstract actor class

// Header file contents
namespace mozilla {
namespace dom {
class GreetParent :
    public PGreetParent
{
	void DoSomething();

    virtual bool
    RecvWorld();

    virtual void
    ActorDestroy(ActorDestroyReason aWhy);

    MOZ_IMPLICIT GreetParent();
    virtual ~GreetParent();
};
} // namespace dom
} // namespace mozilla

#endif //endif mozilla_dom_GreetParent_h
