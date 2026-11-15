#pragma once

#include "StringUtils.h"





/** A namespaced block ID and the properties that specify its state. */
struct cNamedBlockState
{
	AString Name;
	AStringMap Properties;

	bool operator ==(const cNamedBlockState & a_Other) const
	{
		return (Name == a_Other.Name) && (Properties == a_Other.Properties);
	}

	bool operator !=(const cNamedBlockState & a_Other) const
	{
		return !(*this == a_Other);
	}
};
