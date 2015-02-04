
#ifndef mozilla_dom_GreetChild_h
#define mozilla_dom_GreetChild_h

#include "mozilla/dom/PGreetChild.h"

// Header file contents
namespace mozilla {
namespace dom {
class GreetChild :
    public PGreetChild
{
    virtual bool
    RecvHello();

    void
	CallDad();

    MOZ_IMPLICIT GreetChild();
    virtual ~GreetChild();
};
} // namespace dom
} // namespace mozilla


#endif // endif mozilla_dom_GreetChild_h
