//*****************************************************************************
// Libbroadcast: A library for broadcasting video over RTMP
//
// Copyright (C) 2014 Lucas Murray <lmurray@undefinedfire.com>
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

#include "include/rtmptargetinfo.h"
#include <QtCore/QStringList>
#include <QtCore/QUrl>

RTMPTargetInfo::RTMPTargetInfo()
	: protocol(RTMPProtocol)
	, host()
	, port(DEFAULT_RTMP_PORT)
	, appName()
	, appInstance()
	, streamName()
{
}

RTMPTargetInfo::RTMPTargetInfo(
	RTMPProtocolType protocol, const QString &host, int port,
	const QString &appName, const QString &appInstance,
	const QString &streamName)
	: protocol(protocol)
	, host(host)
	, port(port)
	, appName(appName)
	, appInstance(appInstance)
	, streamName(streamName)
{
}

RTMPTargetInfo::RTMPTargetInfo(const RTMPTargetInfo &other)
	: protocol(other.protocol)
	, host(other.host)
	, port(other.port)
	, appName(other.appName)
	, appInstance(other.appInstance)
	, streamName(other.streamName)
{
}

RTMPTargetInfo &RTMPTargetInfo::operator=(const RTMPTargetInfo &other)
{
	protocol = other.protocol;
	host = other.host;
	port = other.port;
	appName = other.appName;
	appInstance = other.appInstance;
	streamName = other.streamName;
	return *this;
}

/// <summary>
/// Parses a URL into a `RTMPTargetInfo` object from the format:
/// "protocol://host[:port]/appName[/appInstance]" or
/// "protocol://host[:port]/appName[/appInstance]/streamName" depending
/// if `incStreamName` is set or not.
/// </summary>
RTMPTargetInfo RTMPTargetInfo::fromUrl(
	const QString &str, bool incStreamName, const QString &streamName)
{
	RTMPTargetInfo out;
	out.protocol = RTMPInvalidProtocol; // Assume invalid until the end

	// Remove whitespace and assume "rtmp://" by default
	QString urlStr = str.trimmed();
	if(!urlStr.contains(QStringLiteral("://")))
		urlStr = "rtmp://" + urlStr;

	// Test if the string is in a valid URL format
	QUrl url(urlStr);
	if(!url.isValid())
		return out;
	bool isValidProtocol = false;
	for(int i = 0; i < NUM_RTMP_PROTOCOL_TYPES; i++) {
		if(url.scheme() == QString(RTMPProtocolTypeStrings[i]))
			isValidProtocol = true;
	}
	if(!isValidProtocol)
		return out;
	if(!url.userInfo().isEmpty())
		return out;
	if(url.host().isEmpty())
		return out;
	if(url.hasFragment())
		return out;
	QString path = url.path().remove(0, 1); // Remove starting "/"
	if(url.hasQuery()) // Our path can include a "?"
		path += "?" + url.query();
	if(path.isEmpty())
		return out;

	// Parse easy URL components
	if(url.scheme() == QString(RTMPProtocolTypeStrings[RTMPProtocol]))
		out.protocol = RTMPProtocol;
	else if(url.scheme() == QString(RTMPProtocolTypeStrings[RTMPSProtocol]))
		out.protocol = RTMPSProtocol;
	out.host = url.host();
	out.port = url.port(DEFAULT_RTMP_PORT);

	// Parse the URL path component. We assume that only the application
	// instance can contain "/" characters.
	QStringList split = path.split(QChar('/'));
	if(incStreamName) {
		if(split.size() > 1) {
			out.streamName = split.last();
			split.removeLast();
		}
	} else
		out.streamName = streamName;
	out.appName = split.first();
	split.removeFirst();
	if(split.size() > 0)
		out.appInstance = split.join(QChar('/'));

	// Validate path components
	if(out.appName.isEmpty())
		out.protocol = RTMPInvalidProtocol;

	return out;
}

/// <summary>
/// Returns the remote target information as a URL in the format:
/// "protocol://host[:port]/appName[/appInstance][/streamName]"
///
/// If `forcePort` is true then the port number will be included even if it is
/// the default port.
/// </summary>
QString RTMPTargetInfo::asUrl(bool forcePort, bool incStreamName) const
{
	QString url;
	if(port == DEFAULT_RTMP_PORT && !forcePort) {
		url = QStringLiteral("%1://%2/%3")
			.arg(RTMPProtocolTypeStrings[(int)protocol])
			.arg(host)
			.arg(appName);
	} else {
		url = QStringLiteral("%1://%2:%3/%4")
			.arg(RTMPProtocolTypeStrings[(int)protocol])
			.arg(host)
			.arg(port)
			.arg(appName);
	}
	if(!appInstance.isEmpty())
		url.append(QStringLiteral("/%1").arg(appInstance));
	if(incStreamName && !streamName.isEmpty())
		url.append(QStringLiteral("/%1").arg(streamName));
	return url;
}
