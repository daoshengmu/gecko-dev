#include "HelloPluginParent.h"

namespace mozilla {
namespace dom {


bool HelloPluginParent::RecvHello()
{
	printf("[HelloPluginParent] in RecvHello()");

	if ( !SendWorld() )
		puts("[HelloPluginParent] SendWorld() fail");

	return true;
}


} // namespace dom
} // namespace mozilla
