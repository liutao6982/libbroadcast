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

#ifndef BROLOG_H
#define BROLOG_H

#include "libbroadcast.h"
#include <QtCore/QVector>

class BroLogData;
class QPoint;
class QPointF;
class QRect;
class QRectF;
class QSize;
class QSizeF;

//=============================================================================
class BroLog
{
public: // Datatypes ----------------------------------------------------------
	enum LogLevel {
		Notice = 0,
		Warning,
		Critical
	};

	typedef void CallbackFunc(
		const QString &cat, const QString &msg, LogLevel lvl);

protected: // Static members --------------------------------------------------
	static CallbackFunc *	s_callbackFunc;

public: // Members ------------------------------------------------------------
	BroLogData *	d;

public: // Static methods -----------------------------------------------------
	LBC_EXPORT static void	setCallback(CallbackFunc *funcPtr);

public: // Constructor/destructor ---------------------------------------------
	BroLog();
	BroLog(const BroLog &log);
	~BroLog();
};
BroLog operator<<(BroLog log, const QString &msg);
BroLog operator<<(BroLog log, const QByteArray &msg);
BroLog operator<<(BroLog log, const char *msg);
BroLog operator<<(BroLog log, int msg);
BroLog operator<<(BroLog log, unsigned int msg);
BroLog operator<<(BroLog log, qint64 msg);
BroLog operator<<(BroLog log, quint64 msg);
BroLog operator<<(BroLog log, qreal msg);
BroLog operator<<(BroLog log, float msg);
BroLog operator<<(BroLog log, bool msg);
BroLog operator<<(BroLog log, const QPoint &msg);
BroLog operator<<(BroLog log, const QPointF &msg);
BroLog operator<<(BroLog log, const QRect &msg);
BroLog operator<<(BroLog log, const QRectF &msg);
BroLog operator<<(BroLog log, const QSize &msg);
BroLog operator<<(BroLog log, const QSizeF &msg);
BroLog broLog(const QString &category, BroLog::LogLevel lvl = BroLog::Notice);
BroLog broLog(BroLog::LogLevel lvl = BroLog::Notice);
//=============================================================================

inline void BroLog::setCallback(CallbackFunc *funcPtr)
{
	s_callbackFunc = funcPtr;
}

#endif // BROLOG_H
