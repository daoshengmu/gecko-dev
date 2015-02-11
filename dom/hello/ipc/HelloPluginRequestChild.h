
#ifndef mozilla_dom_HelloPluginRequestChild_h
#define mozilla_dom_HelloPluginRequestChild_h

#include "mozilla/dom/PHelloPluginRequestChild.h"

// Header file contents

namespace mozilla {
namespace dom {

class HelloPluginRequestChild :
    public PHelloPluginRequestChild
{

public:
    MOZ_IMPLICIT HelloPluginRequestChild();
    virtual ~HelloPluginRequestChild();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HelloPluginRequestChild_h
