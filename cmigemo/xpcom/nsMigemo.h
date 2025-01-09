// Copyright 2005 MURAOKA Taro(KoRoN)/KaoriYa

#ifndef NS_MIGEMO_H
#define NS_MIGEMO_H

#include "nsIMigemo.h"
#include "migemo.h"

#define NS_MIGEMO_CONTRACTID	"@kaoriya.net/migemo/nsMigemo;1"
#define NS_MIGEMO_CLASSNAME	"Migemo XPCOM Wrapper"
#define NS_MIGEMO_CID		{0x26a1d4f8, 0x9fbe, 0x44de, \
    { 0x90, 0xb6, 0x69, 0x09, 0xc8, 0x2c, 0x5f, 0xd1 }}

class nsMigemo : public nsIMigemo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMIGEMO

  nsMigemo();

private:
  ~nsMigemo();

private:
  static migemo* migemoObject;
};

#endif /*NS_MIGEMO_H*/
