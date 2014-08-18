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

#include "include/libbroadcast.h"
#include "include/rtmpclient.h"
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

//=============================================================================
// Helpers

#ifdef Q_OS_WIN
/// <summary>
/// Converts a `QString` into a `wchar_t` array. We cannot use
/// `QString::toStdWString()` as it's not enabled in our Qt build. The caller
/// is responsible for calling `delete[]` on the returned string.
/// </summary>
static wchar_t *QStringToWChar(const QString &str)
{
	// We must keep the QByteArray in memory as otherwise its `data()` gets
	// freed and we end up converting undefined data
	QByteArray data = str.toUtf8();
	const char *msg = data.data();
	int len = MultiByteToWideChar(CP_UTF8, 0, msg, -1, NULL, 0);
	wchar_t *wstr = new wchar_t[len];
	MultiByteToWideChar(CP_UTF8, 0, msg, -1, wstr, len);
	return wstr;
}
#endif

/// <summary>
/// Displays a very basic error dialog box. Used as a last resort.
/// </summary>
static void showBasicErrorMessageBox(
	const QString &msg, const QString &caption)
{
#ifdef Q_OS_WIN
	wchar_t *wMessage = QStringToWChar(msg);
	wchar_t *wCaption = QStringToWChar(caption);
	MessageBox(NULL, wMessage, wCaption, MB_OK | MB_ICONERROR);
	delete[] wMessage;
	delete[] wCaption;
#else
#error Unsupported platform
#endif
}

//=============================================================================
// Library initialization

bool initLibbroadcast_internal(
	int libVerMajor, int libVerMinor, int libVerPatch)
{
	static bool inited = false;
	if(inited)
		return false; // Already initialized
	inited = true;

	// Test Libbroadcast version. TODO: When the API is stable we should not
	// test to see if the patch version is the same
	if(libVerMajor != LIBBROADCAST_VER_MAJOR ||
		libVerMinor != LIBBROADCAST_VER_MINOR ||
		libVerPatch != LIBBROADCAST_VER_BUILD)
	{
		QString msg = QStringLiteral(
			"Fatal: Mismatched Libbroadcast version!");

		// Output to the terminal
		QByteArray output = msg.toLocal8Bit();
		std::cout << output.constData() << std::endl;

		// Visual Studio does not display stdout in the debug console so we
		// need to use a special Windows API
#if defined(Q_OS_WIN) && defined(QT_DEBUG)
		// Output to the Visual Studio or system debugger in debug builds only
		OutputDebugStringA(output.constData());
		OutputDebugStringA("\r\n");
#endif

		// Display a message box so the user knows something went wrong
		showBasicErrorMessageBox(msg, QStringLiteral("Libbroadcast"));

		return false;
	}

	// Register our signal datatypes with Qt
	qRegisterMetaType<RTMPClient::RTMPError>();

	return true;
}
