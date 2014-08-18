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

#include <gtest/gtest.h>
#include <Libbroadcast/libbroadcast.h>
#include <Libbroadcast/brolog.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

//=============================================================================
// Log handlers

void broLogHandler(
	const QString &cat, const QString &msg, BroLog::LogLevel lvl)
{
	QString	formattedMsg;

	// Generate log entry string
	QString prefix;
	if(!cat.isEmpty())
		prefix.append(QStringLiteral("[%1]").arg(cat));
	switch(lvl) {
	default:
	case BroLog::Notice:
		if(!prefix.isEmpty())
			formattedMsg = QStringLiteral("%1 %2").arg(prefix).arg(msg);
		else
			formattedMsg = msg;
		break;
	case BroLog::Warning:
		formattedMsg = QStringLiteral("%1[!!] %2").arg(prefix).arg(msg);
		break;
	case BroLog::Critical:
		formattedMsg =
			QStringLiteral("%1[!!!!!] %2").arg(prefix).arg(msg);
		break;
	}

	// Output to the terminal
	QByteArray output = formattedMsg.toLocal8Bit();
	std::cout << output.constData() << std::endl;

	// Visual Studio does not display stdout in the debug console so we
	// need to use a special Windows API
#if defined(Q_OS_WIN) && defined(QT_DEBUG)
	// Output to the Visual Studio or system debugger in debug builds only
	OutputDebugStringA(output.constData());
	OutputDebugStringA("\r\n");
#endif
}

void appLog(const QString &msg)
{
	broLogHandler(QString(), msg, BroLog::Notice);
}

void qtMessageHandler(
	QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	BroLog::LogLevel lvl;

	switch(type) {
	case QtDebugMsg:
		lvl = BroLog::Notice;
		break;
	default:
	case QtWarningMsg:
		lvl = BroLog::Warning;
		break;
	case QtCriticalMsg:
	case QtFatalMsg:
		lvl = BroLog::Critical;
		break;
	}

	broLogHandler(QStringLiteral("Qt"), msg, lvl);

#ifdef QT_DEBUG
	if(type == QtFatalMsg) {
		// Cause a SEGFAULT to make it easier for us to debug
		int *tmp = NULL;
		*tmp = 0;
	}
#endif
}

//=============================================================================
// Main entry point

int main(int argc, char *argv[])
{
	// Initialize Libbroadcast
	INIT_LIBBROADCAST();

	// Setup logging
	qInstallMessageHandler(qtMessageHandler);
	BroLog::setCallback(&broLogHandler);
	appLog("Beginning tests...");

	// Initialize Qt and seed PRNG
	QCoreApplication app(argc, argv);
	qsrand(QDateTime::currentMSecsSinceEpoch() % UINT_MAX);

	// Begin tests
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
