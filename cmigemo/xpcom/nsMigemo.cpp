// Copyright 2005 MURAOKA Taro(KoRoN)/KaoriYa

#include "nsMigemo.h"
#include "nsStringAPI.h"

#define MIGEMO_DICTPATH "C:/migemo/utf-8.d/migemo-dict"

NS_IMPL_ISUPPORTS1(nsMigemo, nsIMigemo);

migemo* nsMigemo::migemoObject = NULL;

nsMigemo::nsMigemo()
{
    if (nsMigemo::migemoObject == NULL)
    {
	migemo* obj = migemo_open(MIGEMO_DICTPATH);
	if (obj != NULL && migemo_is_enable(obj) != 0)
	{
	    migemo_set_operator(obj, MIGEMO_OPINDEX_NEST_IN, (const unsigned char*)"(?:");
	    migemo_set_operator(obj, MIGEMO_OPINDEX_NEWLINE, (const unsigned char*)"\\s*");
	    nsMigemo::migemoObject = obj;
	}
    }
}

nsMigemo::~nsMigemo()
{
}

/* AUTF8String query (in AUTF8String key); */
NS_IMETHODIMP
nsMigemo::Query(const nsACString & key, nsACString & _retval)
{
    if (nsMigemo::migemoObject == NULL)
	return NS_ERROR_NOT_INITIALIZED;
    unsigned char* answer = migemo_query(nsMigemo::migemoObject,
	    (const unsigned char*)key.BeginReading());
    if (answer == NULL)
	return NS_ERROR_FAILURE;
    _retval.Assign((const nsACString::char_type*)answer);
    migemo_release(nsMigemo::migemoObject, answer);
    return NS_OK;
}
