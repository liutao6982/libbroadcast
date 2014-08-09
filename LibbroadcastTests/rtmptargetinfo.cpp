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

#include <gtest/gtest.h>
#include <Libbroadcast/rtmptargetinfo.h>

// Compare QStrings with a readable failure message
#define EXPECT_QSTREQ(a, b) \
	EXPECT_STREQ((a).toUtf8().constData(), (b).toUtf8().constData());

TEST(RTMPTargetInfoTest, Domain)
{
	QString url("rtmp://www.mishira.com/live");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	EXPECT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("www.mishira.com"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString(), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IP)
{
	QString url("rtmp://192.168.1.1/live");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString(), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, DomainWithPort)
{
	QString url("rtmp://www.mishira.com:1934/live");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("www.mishira.com"), info.host);
	EXPECT_EQ(1934, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString(), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, DomainNoProtocol)
{
	QString url("www.mishira.com/live");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(QString("rtmp://") + url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("www.mishira.com"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString(), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithPortNoProtocol)
{
	QString url("192.168.1.1:1934/live");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(QString("rtmp://") + url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(1934, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString(), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithTrailingSlash)
{
	// TODO: Should we keep trailing slashes or not?
	QString url("rtmp://192.168.1.1/live/");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(QString("rtmp://192.168.1.1/live"), info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString(), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithWhitespace)
{
	QString url("\trtmp://192.168.1.1/live  ");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(QString("rtmp://192.168.1.1/live"), info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString(), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithQuery)
{
	QString url("rtmp://192.168.1.1/live?backup=1");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live?backup=1"), info.appName);
	EXPECT_QSTREQ(QString(), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithInstance)
{
	QString url("rtmp://192.168.1.1/live/instance");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString("instance"), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithInstanceAndQueryInApp)
{
	QString url("rtmp://192.168.1.1/live?backup=1/instance");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live?backup=1"), info.appName);
	EXPECT_QSTREQ(QString("instance"), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithInstanceAndQueryInInstance)
{
	QString url("rtmp://192.168.1.1/live/instance?backup=1");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString("instance?backup=1"), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithDoubleInstance)
{
	QString url("rtmp://192.168.1.1/live/instance/second");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl());
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString("instance/second"), info.appInstance);
	EXPECT_QSTREQ(QString(), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithStreamName)
{
	QString url("rtmp://192.168.1.1/live/streamName");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl(false, true));
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString(), info.appInstance);
	EXPECT_QSTREQ(QString("streamName"), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithInstanceAndStreamName)
{
	QString url("rtmp://192.168.1.1/live/instance/streamName");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl(false, true));
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString("instance"), info.appInstance);
	EXPECT_QSTREQ(QString("streamName"), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithEmptyInstanceAndStreamName)
{
	QString url("rtmp://192.168.1.1/live//streamName");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(QString("rtmp://192.168.1.1/live/streamName"),
		info.asUrl(false, true));
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString(""), info.appInstance);
	EXPECT_QSTREQ(QString("streamName"), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithDoubleInstanceAndStreamName)
{
	QString url("rtmp://192.168.1.1/live/instance/second/streamName");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl(false, true));
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString("instance/second"), info.appInstance);
	EXPECT_QSTREQ(QString("streamName"), info.streamName);
}

TEST(RTMPTargetInfoTest, IPWithDoubleInstanceAndStreamNameAndQueryInStreamName)
{
	QString url("rtmp://192.168.1.1/live/instance/second/stream?Name");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl(false, true));
	EXPECT_EQ(RTMPProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(DEFAULT_RTMP_PORT, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString("instance/second"), info.appInstance);
	EXPECT_QSTREQ(QString("stream?Name"), info.streamName);
}

TEST(RTMPTargetInfoTest, ParseTest)
{
	QString url("rtmps://192.168.1.1:1934/live/instance/second/streamName");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_TRUE(info.isValid());
	EXPECT_QSTREQ(url, info.asUrl(false, true));
	EXPECT_EQ(RTMPSProtocol, info.protocol);
	EXPECT_QSTREQ(QString("192.168.1.1"), info.host);
	EXPECT_EQ(1934, info.port);
	EXPECT_QSTREQ(QString("live"), info.appName);
	EXPECT_QSTREQ(QString("instance/second"), info.appInstance);
	EXPECT_QSTREQ(QString("streamName"), info.streamName);
}

TEST(RTMPTargetInfoTest, InvalidEmpty)
{
	QString url("");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidEmail)
{
	QString url("user@example.com");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidUserInfo)
{
	QString url("rtmp://user@192.168.1.1:1934/live");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidUserInfoWithPass)
{
	QString url("rtmp://user:pass@192.168.1.1:1934/live");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidDoublePort)
{
	QString url("rtmp://192.168.1.1:1935:1934/live");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidEmptyApp)
{
	QString url("rtmp://192.168.1.1/");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidDoubleSlash)
{
	QString url("rtmp://192.168.1.1//");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidEmptyAppDoubleSlash)
{
	QString url("rtmp://192.168.1.1//instance");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidTripleSlash)
{
	QString url("rtmp://192.168.1.1///");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidEmptyAppTripleSlash)
{
	QString url("rtmp://192.168.1.1///streamName");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidEmptyAppNoSlash)
{
	QString url("rtmp://192.168.1.1");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}

TEST(RTMPTargetInfoTest, InvalidUnknownProtocol)
{
	QString url("http://192.168.1.1");
	RTMPTargetInfo info = RTMPTargetInfo::fromUrl(url, true);
	ASSERT_FALSE(info.isValid());
}
