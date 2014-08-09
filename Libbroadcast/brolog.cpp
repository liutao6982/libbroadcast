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

#include "include/brolog.h"
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtCore/QStringList>

//=============================================================================
// BroLogData class

class BroLogData
{
public: // Members ------------------------------------------------------------
	int					ref;
	QString				cat;
	BroLog::LogLevel	lvl;
	QString				msg;

public: // Constructor/destructor ---------------------------------------------
	BroLogData()
		: ref(0)
		, cat()
		, lvl(BroLog::Notice)
		, msg()
	{
	}
};

//=============================================================================
// BroLog class

static void defaultLog(
	const QString &cat, const QString &msg, BroLog::LogLevel lvl)
{
	// No-op
}
BroLog::CallbackFunc *BroLog::s_callbackFunc = &defaultLog;

BroLog::BroLog()
	: d(new BroLogData())
{
	d->ref++;
}

BroLog::BroLog(const BroLog &log)
	: d(log.d)
{
	d->ref++;
}

BroLog::~BroLog()
{
	d->ref--;
	if(d->ref)
		return; // Temporary object

	// Forward to the callback
	if(s_callbackFunc != NULL)
		s_callbackFunc(d->cat, d->msg, d->lvl);

	delete d;
}

BroLog operator<<(BroLog log, const QString &msg)
{
	log.d->msg.append(msg);
	return log;
}

BroLog operator<<(BroLog log, const QByteArray &msg)
{
	return log << QString::fromUtf8(msg);
}

BroLog operator<<(BroLog log, const char *msg)
{
	return log << QString(msg);
}

BroLog operator<<(BroLog log, int msg)
{
	return log << QString::number(msg);
}

BroLog operator<<(BroLog log, unsigned int msg)
{
	return log << QString::number(msg);
}

BroLog operator<<(BroLog log, qint64 msg)
{
	return log << QString::number(msg);
}

BroLog operator<<(BroLog log, quint64 msg)
{
	return log << QString::number(msg);
}

BroLog operator<<(BroLog log, qreal msg)
{
	return log << QString::number(msg);
}

BroLog operator<<(BroLog log, float msg)
{
	return log << QString::number(msg);
}

BroLog operator<<(BroLog log, bool msg)
{
	if(msg)
		return log << QStringLiteral("true");
	return log << QStringLiteral("false");
}

BroLog operator<<(BroLog log, const QPoint &msg)
{
	return log << QStringLiteral("Point(%1, %2)")
		.arg(msg.x())
		.arg(msg.y());
}

BroLog operator<<(BroLog log, const QPointF &msg)
{
	return log << QStringLiteral("Point(%1, %2)")
		.arg(msg.x())
		.arg(msg.y());
}

BroLog operator<<(BroLog log, const QRect &msg)
{
	return log << QStringLiteral("Rect(%1, %2, %3, %4)")
		.arg(msg.left())
		.arg(msg.top())
		.arg(msg.width())
		.arg(msg.height());
}

BroLog operator<<(BroLog log, const QRectF &msg)
{
	return log << QStringLiteral("Rect(%1, %2, %3, %4)")
		.arg(msg.left())
		.arg(msg.top())
		.arg(msg.width())
		.arg(msg.height());
}

BroLog operator<<(BroLog log, const QSize &msg)
{
	return log << QStringLiteral("Size(%1, %2)")
		.arg(msg.width())
		.arg(msg.height());
}

BroLog operator<<(BroLog log, const QSizeF &msg)
{
	return log << QStringLiteral("Size(%1, %2)")
		.arg(msg.width())
		.arg(msg.height());
}

BroLog broLog(const QString &category, BroLog::LogLevel lvl)
{
	BroLog log;
	log.d->cat = category;
	log.d->lvl = lvl;
	return log;
}

BroLog broLog(BroLog::LogLevel lvl)
{
	return broLog("", lvl);
}
