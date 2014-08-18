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

#ifndef RTMPTARGETINFO_H
#define RTMPTARGETINFO_H

#include "libbroadcast.h"
#include <QtCore/QString>

const int DEFAULT_RTMP_PORT = 1935;

enum RTMPProtocolType {
	RTMPProtocol = 0,
	RTMPSProtocol,

	NUM_RTMP_PROTOCOL_TYPES, // Must be second last
	RTMPInvalidProtocol = 100 // Must be last
};
static const char * const RTMPProtocolTypeStrings[] = {
	"rtmp",
	"rtmps"
};

//=============================================================================
class LBC_EXPORT RTMPTargetInfo
{
public: // Members ------------------------------------------------------------
	RTMPProtocolType	protocol;
	QString				host;
	int					port;
	QString				appName;
	QString				appInstance;
	QString				streamName;

public: // Static methods -----------------------------------------------------
	static RTMPTargetInfo	fromUrl(
		const QString &url, bool incStreamName = false,
		const QString &streamName = QString());

public: // Constructor/destructor ---------------------------------------------
	RTMPTargetInfo();
	RTMPTargetInfo(
		RTMPProtocolType protocol, const QString &host, int port,
		const QString &appName, const QString &appInstance,
		const QString &streamName);
	RTMPTargetInfo(const RTMPTargetInfo &other);
	RTMPTargetInfo &operator=(const RTMPTargetInfo &other);

public: // Methods ------------------------------------------------------------
	bool	isValid() const;
	QString	asUrl(bool forcePort = false, bool incStreamName = false) const;
};
//=============================================================================

inline bool RTMPTargetInfo::isValid() const
{
	return protocol != RTMPInvalidProtocol;
}

#endif // RTMPTARGETINFO_H
