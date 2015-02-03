
#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "nsTest.h"


NS_GENERIC_FACTORY_CONSTRUCTOR(nsTest)


NS_DEFINE_NAMED_CID(NS_TEST_CID);

static const mozilla::Module::CIDEntry kTestCIDs[] = {
  { &kNS_TEST_CID, false, nullptr, nsTestConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kTestContracts[] = {
  { NS_TEST_CONTRACTID, &kNS_TEST_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kTestCategories[] = {
  { "my-category", "my-key", NS_TEST_CONTRACTID },
  { nullptr }
};

static const mozilla::Module kTestModule = {
  mozilla::Module::kVersion,
  kTestCIDs,
  kTestContracts,
  kTestCategories
};

NSMODULE_DEFN(nsTestModule) = &kTestModule;

NS_IMPL_MOZILLA192_NSGETMODULE(&kTestModule)

