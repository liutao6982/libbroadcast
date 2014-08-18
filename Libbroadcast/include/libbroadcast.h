//*****************************************************************************
// Libbroadcast: A library for broadcasting video over RTMP
//
// Copyright (C) 2014 Lucas Murray <lucas@polyflare.com>
// All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//*****************************************************************************

#ifndef LIBBROADCAST_H
#define LIBBROADCAST_H

#include <QtCore/QRect>

// Export symbols from the DLL while allowing header file reuse by users
#ifdef LIBBROADCAST_LIB
#define LBC_EXPORT Q_DECL_EXPORT
#else
#define LBC_EXPORT Q_DECL_IMPORT
#endif

//=============================================================================
// Global application constants

// Library version. NOTE: Don't forget to update the values in the resource
// files as well ("Libbroadcast.rc")
#define LIBBROADCAST_VER_STR "v0.5.0"
#define LIBBROADCAST_VER_MAJOR 0
#define LIBBROADCAST_VER_MINOR 5
#define LIBBROADCAST_VER_BUILD 0

//=============================================================================
// Library initialization

LBC_EXPORT bool		initLibbroadcast_internal(
	int libVerMajor, int libVerMinor, int libVerPatch);

/// <summary>
/// Initializes Libbroadcast. Must be called as the very first thing in
/// `main()`.
/// </summary>
#define INIT_LIBBROADCAST() \
	if(!initLibbroadcast_internal( \
	LIBBROADCAST_VER_MAJOR, LIBBROADCAST_VER_MINOR, LIBBROADCAST_VER_BUILD)) \
	return 1

#endif // LIBBROADCAST_H
