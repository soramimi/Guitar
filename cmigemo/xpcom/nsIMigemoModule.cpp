// Copyright 2005 MURAOKA Taro(KoRoN)/KaoriYa

#include <nsIGenericFactory.h>
#include "nsMigemo.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMigemo);

static nsModuleComponentInfo components[] = 
{
    {
	NS_MIGEMO_CLASSNAME,
	NS_MIGEMO_CID,
	NS_MIGEMO_CONTRACTID,
	nsMigemoConstructor,
    }
};

NS_IMPL_NSGETMODULE("nsIMigemoModule", components);
