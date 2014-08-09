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
#include <Libbroadcast/amf.h>
#include <Libbroadcast/rtmpclient.h>
#include <QtTest/QSignalSpy>
#include "testdata.h"

#define DO_SLOW_TESTS 0
#define DO_VIDEO_ONLY_TEST_STREAM 0
#define DO_VIDEO_AUDIO_TEST_STREAM 1
#define USE_FMS_BY_DEFAULT 1
const int VIDEO_LENGTH_SECS = 1;

#define FMS_HOST "192.168.1.151"
#define FMS_PORT (DEFAULT_RTMP_PORT - 1)
#define FMS_APP_NAME "live"
#define FMS_STREAM_NAME "testStream"

#define NGINX_HOST "192.168.1.151"
#define NGINX_PORT DEFAULT_RTMP_PORT
#define NGINX_APP_NAME "testApp"
#define NGINX_STREAM_NAME "testStream"

//=============================================================================
// Initial connection tests

class RTMPClientInitializeTest : public testing::Test
{
protected:
	RTMPTargetInfo	m_target;
	RTMPClient *	m_client;
	QSignalSpy *	m_connectingSpy;
	QSignalSpy *	m_connectedSpy;
	QSignalSpy *	m_initializedSpy;
	QSignalSpy *	m_connectedToAppSpy;
	QSignalSpy *	m_createdStreamSpy;
	QSignalSpy *	m_disconnectedSpy;
	QSignalSpy *	m_errorSpy;

	virtual void SetUp()
	{
		// Setup default target
		m_target = defaultTarget();

		// Create client and spies
		m_client = new RTMPClient();
		m_connectingSpy = new QSignalSpy(m_client, SIGNAL(connecting()));
		m_connectedSpy = new QSignalSpy(m_client, SIGNAL(connected()));
		m_initializedSpy = new QSignalSpy(m_client, SIGNAL(initialized()));
		m_connectedToAppSpy =
			new QSignalSpy(m_client, SIGNAL(connectedToApp()));
		m_createdStreamSpy =
			new QSignalSpy(m_client, SIGNAL(createdStream(uint)));
		m_disconnectedSpy = new QSignalSpy(m_client, SIGNAL(disconnected()));
		m_errorSpy =
			new QSignalSpy(m_client, SIGNAL(error(RTMPClient::RTMPError)));
	};

	virtual void TearDown()
	{
		// Delete client and spies
		delete m_connectingSpy;
		delete m_connectedSpy;
		delete m_initializedSpy;
		delete m_connectedToAppSpy;
		delete m_createdStreamSpy;
		delete m_disconnectedSpy;
		delete m_errorSpy;
		delete m_client;
	};

	RTMPTargetInfo defaultTarget()
	{
#if USE_FMS_BY_DEFAULT
		return fmsTarget();
#else
		return nginxTarget();
#endif // USE_FMS_BY_DEFAULT
	};

	RTMPTargetInfo fmsTarget()
	{
		RTMPTargetInfo target;
		target.host = FMS_HOST;
		target.port = FMS_PORT;
		target.appName = FMS_APP_NAME;
		target.streamName = FMS_STREAM_NAME;
		return target;
	};

	RTMPTargetInfo nginxTarget()
	{
		RTMPTargetInfo target;
		target.host = NGINX_HOST;
		target.port = NGINX_PORT;
		target.appName = NGINX_APP_NAME;
		target.streamName = NGINX_STREAM_NAME;
		return target;
	};

	RTMPTargetInfo twitchTarget()
	{
		RTMPTargetInfo target;
		target.host = "live.justin.tv";
		target.port = DEFAULT_RTMP_PORT;
		target.appName = "app";
		target.streamName = "testStream";
		return target;
	};

	// Connect to target expecting to not succeed
	void connectFail(RTMPClient::RTMPError expectedError)
	{
		// Only detect errors that occur in this method
		m_errorSpy->clear();

		// Make sure we're disconnected
		ASSERT_EQ(
			RTMPClient::DisconnectedState, m_client->getHandshakeState());

		// Setup client
		ASSERT_TRUE(m_client->setRemoteTarget(m_target));

		// Attempt connect
		ASSERT_TRUE(m_connectingSpy->isEmpty());
		ASSERT_TRUE(m_client->connect());
		ASSERT_FALSE(m_connectingSpy->isEmpty());
		m_connectingSpy->clear();
		// It's possible to immediately error
		if(m_errorSpy->isEmpty())
			m_errorSpy->wait(65000);
		ASSERT_FALSE(m_errorSpy->isEmpty());
		ASSERT_TRUE(m_connectedSpy->isEmpty());

		// Make sure we received the correct error message
		RTMPClient::RTMPError err =
			m_errorSpy->first().first().value<RTMPClient::RTMPError>();
		ASSERT_EQ(err, expectedError);

		// Make sure we are not still trying to connect
		ASSERT_EQ(
			RTMPClient::DisconnectedState, m_client->getHandshakeState());
	}

	void connect(bool untilInitialized = true, bool untilAppConnect = true)
	{
		// Only detect errors that occur in this method
		m_errorSpy->clear();

		// Make sure we're disconnected
		ASSERT_EQ(
			RTMPClient::DisconnectedState, m_client->getHandshakeState());

		// Setup client
		ASSERT_TRUE(m_client->setRemoteTarget(m_target));
		m_client->setAutoInitialize(untilInitialized);
		m_client->setAutoConnectToApp(untilAppConnect);
		m_client->setVersionString(m_client->getVersionString() +
			QStringLiteral(" MishiraTest/1.99"));

		// Attempt connect
		ASSERT_TRUE(m_connectingSpy->isEmpty());
		ASSERT_TRUE(m_client->connect());
		ASSERT_FALSE(m_connectingSpy->isEmpty());
		m_connectingSpy->clear();
		// It's possible to immediately connect if the target is localhost
		if(m_connectedSpy->isEmpty())
			m_connectedSpy->wait();
		ASSERT_FALSE(m_connectedSpy->isEmpty());
		m_connectedSpy->clear();

		// We should have received no errors
		ASSERT_TRUE(m_errorSpy->isEmpty());

		if(!untilInitialized)
			return;

		// Wait until initialized. It's possible to already be initialized if
		// the target is localhost
		if(m_initializedSpy->isEmpty())
			m_initializedSpy->wait();
		ASSERT_FALSE(m_initializedSpy->isEmpty());
		m_initializedSpy->clear();

		if(!untilAppConnect)
			return;

		// Wait until the RTMP application has connected. It's possible to
		// already be connected if the target is localhost
		if(m_connectedToAppSpy->isEmpty())
			m_connectedToAppSpy->wait();
		ASSERT_FALSE(m_connectedToAppSpy->isEmpty());
		m_connectedToAppSpy->clear();

		// TODO: Test for invalid app

		// We should have received no errors
		ASSERT_TRUE(m_errorSpy->isEmpty());
	};

	void disconnect()
	{
		// Only detect errors that occur in this method
		m_errorSpy->clear();

		// Make sure we're already connected
		ASSERT_NE(
			RTMPClient::DisconnectedState, m_client->getHandshakeState());

		// Disconnect cleanly
		ASSERT_TRUE(m_disconnectedSpy->isEmpty());
		m_client->disconnect();
		// It's possible to immediately disconnect
		if(m_disconnectedSpy->isEmpty())
			m_disconnectedSpy->wait();
		ASSERT_FALSE(m_disconnectedSpy->isEmpty());
		m_disconnectedSpy->clear();

		// We should have received no errors
		ASSERT_TRUE(m_errorSpy->isEmpty());
	};

	// This is a private method in RTMPClient
	bool writeCreateStreamMsg()
	{
		return m_client->writeCreateStreamMsg();
	};

	// This is a private method in RTMPClient
	bool writeDeleteStreamMsg(uint streamId)
	{
		return m_client->writeDeleteStreamMsg(streamId);
	};
};

TEST_F(RTMPClientInitializeTest, ConnectToInvalidHost)
{
	m_target.host = "aslkdfjhakdsljfh.lan";
	connectFail(RTMPClient::HostNotFoundError);
}

#if DO_SLOW_TESTS
TEST_F(RTMPClientInitializeTest, ConnectToInactivePort)
{
	m_target.port = 1936;
	connectFail(RTMPClient::ConnectionRefusedError);
}

TEST_F(RTMPClientInitializeTest, ConnectToTcpRstPort)
{
	m_target.port = 1937;
	connectFail(RTMPClient::ConnectionRefusedError);
}

TEST_F(RTMPClientInitializeTest, ConnectToIcmpRejectPort)
{
	m_target.port = 1938;
	connectFail(RTMPClient::NetworkError);
}
#endif // DO_SLOW_TESTS

TEST_F(RTMPClientInitializeTest, ConnectToValidHost)
{
	connect(false, false);
	disconnect();
}

TEST_F(RTMPClientInitializeTest, FullyInitialize)
{
	connect();
	disconnect();
}

#if 0
TEST_F(RTMPClientInitializeTest, CreateStreamBeforeConnect)
{
	// Creating a stream before connecting to an application has unpredictable
	// results. nginx-rtmp creates a new stream and returns a result, FMS
	// issues an "_error()" and "close()" message for every command sent and
	// Twitch doesn't respond at all.

	//m_target = twitchTarget();
	connect(true, false);
	ASSERT_TRUE(writeCreateStreamMsg());

	// TODO: Wait for message received
	m_errorSpy->wait(); // HACK

	disconnect();
}
#endif // 0

TEST_F(RTMPClientInitializeTest, ConnectToInvalidApp)
{
	// This tests assumes that the server will instantly disconnect the client
	// if it attempts to connect to an invalid application or that the server
	// will issue an "_error()" and that our library will then issue a
	// disconnect. nginx-rtmp instantly disconnects while FMS and the Twitch
	// servers reply with an "_error()" and "close()" messages.

	//m_target = twitchTarget();
	m_target.appName = "aslkdfjhakdsljfh";
	connect(true, false);

	ASSERT_TRUE(m_disconnectedSpy->isEmpty());
	ASSERT_TRUE(m_client->connectToApp());
	// It's possible to immediately disconnect
	if(m_disconnectedSpy->isEmpty())
		m_disconnectedSpy->wait();
	ASSERT_FALSE(m_disconnectedSpy->isEmpty());
	m_disconnectedSpy->clear();
}

//=============================================================================
// After handshake and RTMP "connect()" tests

class RTMPClientConnectedTest : public RTMPClientInitializeTest
{
protected:
	RTMPPublisher *	m_publisher;
	int				m_nextVidFrame;
	int				m_nextAudFrame;

	virtual void SetUp()
	{
		RTMPClientInitializeTest::SetUp();
		m_publisher = NULL;
		m_nextVidFrame = 0;
		m_nextAudFrame = 0;
	};

	virtual void TearDown()
	{
		RTMPClientInitializeTest::TearDown();
	};

	void createPublisher()
	{
		// Create publisher object
		m_publisher = m_client->createPublishStream();
		ASSERT_FALSE(m_publisher == NULL);
		QSignalSpy readySpy(m_publisher, SIGNAL(ready()));

		// Create actual publish stream and call "publish()"
		ASSERT_TRUE(m_publisher->beginPublishing());
		if(readySpy.isEmpty())
			readySpy.wait();
		ASSERT_FALSE(readySpy.isEmpty());
	};

	void deletePublisher()
	{
		// Delete publish stream cleanly
		ASSERT_TRUE(m_publisher->finishPublishing());
		m_client->deletePublishStream();
	};

	QByteArray getNextVidFrame(QByteArray &headerOut, quint32 &timestampOut)
	{
		// Map absolute frame number to our test frames
		int frame = m_nextVidFrame % NUM_TEST_VIDEO_FRAMES;

		// Create FLV "VideoTagHeader" structure
		char header[5];
		if(frame == 0) {
			// SEI+IDR pair
			header[0] = 0x17; // AVC keyframe
			header[1] = 0x01; // AVC NALU
			// Composition time = PTS - DTS in msec
			amfEncodeUInt24(&header[2],
				testVidFrameTSDiffs[frame] * 1000 / TEST_VIDEO_FRAME_RATE);
		} else {
			// P/B slice
			header[0] = 0x27; // AVC interframe
			header[1] = 0x01; // AVC NALU
			// Composition time = PTS - DTS in msec
			amfEncodeUInt24(&header[2],
				testVidFrameTSDiffs[frame] * 1000 / TEST_VIDEO_FRAME_RATE);
		}
		headerOut = QByteArray(header, sizeof(header));

		// Determine frame timestamp
		timestampOut = m_nextVidFrame * 1000 / TEST_VIDEO_FRAME_RATE;

		// Get frame data
		QByteArray data = QByteArray::fromRawData(
			reinterpret_cast<const char *>(testVidFrames[frame]),
			testVidFrameSizes[frame]);

		m_nextVidFrame++;
		return data;
	};

	quint32 timestampOfNextAudFrame()
	{
		return m_nextAudFrame * 1000 / TEST_AUDIO_FRAME_RATE;
	}

	QByteArray getNextAudFrame(QByteArray &headerOut, quint32 &timestampOut)
	{
		// Map absolute frame number to our test frames
		int frame = m_nextAudFrame % NUM_TEST_AUDIO_FRAMES;

		// Create FLV "AudioTagHeader" structure
		char header[2];
		header[0] = (char)0xAF; // AAC format (Constant)
		header[1] = (char)0x01; // 0 = AAC sequence header, 1 = AAC data
		headerOut = QByteArray(header, sizeof(header));

		// Determine frame timestamp
		timestampOut = timestampOfNextAudFrame();

		// Get frame data
		QByteArray data = QByteArray::fromRawData(
			reinterpret_cast<const char *>(testAudFrames[frame]),
			testAudFrameSizes[frame]);

		m_nextAudFrame++;
		return data;
	};
};

TEST_F(RTMPClientConnectedTest, CreateDeleteStream)
{
	connect();

	// Create stream
	ASSERT_TRUE(m_createdStreamSpy->isEmpty());
	ASSERT_TRUE(writeCreateStreamMsg());
	if(m_createdStreamSpy->isEmpty())
		m_createdStreamSpy->wait();
	ASSERT_FALSE(m_createdStreamSpy->isEmpty());
	uint streamId = m_createdStreamSpy->first().first().value<uint>();
	ASSERT_NE(0, streamId);
	EXPECT_EQ(1, streamId);

	// Delete stream. As the server doesn't send a response we can't test it to
	// make sure that it worked
	ASSERT_TRUE(writeDeleteStreamMsg(streamId));

	disconnect();
}

#if 0
TEST_F(RTMPClientConnectedTest, PingResponse)
{
	// This test assumes that the nginx-rtmp server's ping interval is 20
	// seconds, ping timeout is 10 seconds and actually disconnects the client
	// if it doesn't reply to a ping. At the time of writing this test
	// nginx-rtmp was broken and did not disconnect clients.

	m_target = nginxTarget();
	connect();

	// Wait for 60 seconds and see if we're still connected
	ASSERT_TRUE(m_disconnectedSpy->isEmpty());
	m_disconnectedSpy->wait(60000);
	ASSERT_TRUE(m_disconnectedSpy->isEmpty());

	disconnect();
}
#endif // 0

TEST_F(RTMPClientConnectedTest, CreatePublishStream)
{
	//m_target = twitchTarget();
	//m_target = nginxTarget();
	connect();
	createPublisher();
	deletePublisher();
	disconnect();
}

#if DO_VIDEO_ONLY_TEST_STREAM
TEST_F(RTMPClientConnectedTest, WriteVideoOnlyStream)
{
	//m_target = twitchTarget();
	//m_target = nginxTarget();
	connect();
	createPublisher();

	// Only detect errors that occur in this method
	m_errorSpy->clear();

	// Write "@setDataFrame()"
	AMFObject obj;
	// Video information
	obj["videocodecid"] = new AMFString("avc1");
	obj["videodatarate"] = new AMFNumber(100.0);
	obj["width"] = new AMFNumber(64.0);
	obj["height"] = new AMFNumber(36.0);
	obj["framerate"] = new AMFNumber(TEST_VIDEO_FRAME_RATE);
	//obj["videokeyframe_frequency"] = new AMFNumber(2.0);
	// Audio information
	//obj["audiocodecid"] = new AMFString("mp4a");
	//obj["audiodatarate"] = new AMFNumber(96.0);
	//obj["audiosamplerate"] = new AMFNumber(44100.0);
	//obj["audiochannels"] = new AMFNumber(2.0);
	// Extra information
	//obj["encoder"] = new AMFString("MishiraTest/1.99");
	//obj["duration"] = new AMFNumber(0.0);
	//obj["filesize"] = new AMFNumber(0.0);
	ASSERT_TRUE(m_publisher->writeDataFrame(&obj));
	// It's possible to immediately error
	if(m_errorSpy->isEmpty())
		m_errorSpy->wait(500);
	ASSERT_TRUE(m_errorSpy->isEmpty());

	// Limit the amount of data that can be pending for write so that some
	// messages must be transmitted across several packets
	const int OS_BUF_SIZE = 1000;
	ASSERT_NE(-1, m_client->setOSWriteBufferSize(OS_BUF_SIZE));
	int actualSize = m_client->getOSWriteBufferSize();
	ASSERT_LE(OS_BUF_SIZE, actualSize);
	ASSERT_GE(OS_BUF_SIZE * 2, actualSize);

	// Write "AVCDecoderConfigurationRecord"
	ASSERT_TRUE(m_publisher->writeAvcConfigRecord(
		QByteArray::fromRawData(testVidFrame0SPS, sizeof(testVidFrame0SPS)),
		QByteArray::fromRawData(testVidFrame0PPS, sizeof(testVidFrame0PPS))));

	// Write video data
	for(int i = 0; i < VIDEO_LENGTH_SECS * TEST_VIDEO_FRAME_RATE + 1; i++) {
		// Get frame data
		quint32 timestamp;
		QByteArray header;
		QByteArray data = getNextVidFrame(header, timestamp);

		// We want to pretend we are recording the video in real time but need
		// to make sure that the OS is ready for the data
		if(!m_publisher->willWriteBuffer())
			_sleep(1000 / TEST_VIDEO_FRAME_RATE);
		else
			while(m_publisher->willWriteBuffer())
				m_errorSpy->wait(100); // HACK: Sleep while processing Qt

		// Write video frame
		if(timestamp == 0) {
			// First IDR should be prefixed by an SEI
			QVector<QByteArray> pkts;
			pkts.append(QByteArray::fromRawData(
				testVidFrame0SEI, sizeof(testVidFrame0SEI)));
			pkts.append(data);
			ASSERT_TRUE(
				m_publisher->writeVideoFrame(timestamp, header, pkts));
		} else {
			QVector<QByteArray> pkts;
			pkts.append(data);
			ASSERT_TRUE(
				m_publisher->writeVideoFrame(timestamp, header, pkts));
		}
	}

	// Make sure that we didn't have any errors
	if(m_errorSpy->isEmpty())
		m_errorSpy->wait(500);
	ASSERT_TRUE(m_errorSpy->isEmpty());

	deletePublisher();
	disconnect();
}
#endif // DO_VIDEO_ONLY_TEST_STREAM

#if DO_VIDEO_AUDIO_TEST_STREAM
TEST_F(RTMPClientConnectedTest, WriteVideoAudioStream)
{
	//m_target = twitchTarget();
	//m_target = nginxTarget();
	connect();
	createPublisher();

	// Only detect errors that occur in this method
	m_errorSpy->clear();

	// Be more efficient in network usage
	m_publisher->beginForceBufferWrite();

	// Write "@setDataFrame()"
	AMFObject obj;
	// Video information
	obj["videocodecid"] = new AMFString("avc1");
	obj["videodatarate"] = new AMFNumber(100.0);
	obj["width"] = new AMFNumber(64.0);
	obj["height"] = new AMFNumber(36.0);
	obj["framerate"] = new AMFNumber(TEST_VIDEO_FRAME_RATE);
	//obj["videokeyframe_frequency"] = new AMFNumber(2.0);
	// Audio information
	obj["audiocodecid"] = new AMFString("mp4a");
	obj["audiodatarate"] = new AMFNumber(128.0);
	obj["audiosamplerate"] = new AMFNumber(44100.0);
	obj["audiochannels"] = new AMFNumber(2.0);
	// Extra information
	//obj["encoder"] = new AMFString("MishiraTest/1.99");
	//obj["duration"] = new AMFNumber(0.0);
	//obj["filesize"] = new AMFNumber(0.0);
	ASSERT_TRUE(m_publisher->writeDataFrame(&obj));
	// It's possible to immediately error
	if(m_errorSpy->isEmpty())
		m_errorSpy->wait(500);
	ASSERT_TRUE(m_errorSpy->isEmpty());

	// Write "AVCDecoderConfigurationRecord"
	ASSERT_TRUE(m_publisher->writeAvcConfigRecord(
		QByteArray::fromRawData(
		reinterpret_cast<const char *>(testVidFrame0SPS),
		sizeof(testVidFrame0SPS)),
		QByteArray::fromRawData(
		reinterpret_cast<const char *>(testVidFrame0PPS),
		sizeof(testVidFrame0PPS))));

	// Write AAC sequence header
	ASSERT_TRUE(m_publisher->writeAacSequenceHeader(
		QByteArray::fromRawData(reinterpret_cast<const char *>(testAudOob),
		sizeof(testAudOob))));

	// Write video and audio data
	for(int i = 0; i < VIDEO_LENGTH_SECS * TEST_VIDEO_FRAME_RATE + 1; i++) {
		// Get frame data
		quint32 timestamp;
		QByteArray header;
		QByteArray data = getNextVidFrame(header, timestamp);

		// Write as many audio frames that are before the current video frame
		while(timestampOfNextAudFrame() < timestamp) {
			// Get frame data
			quint32 audTimestamp;
			QByteArray audHeader;
			QByteArray audData = getNextAudFrame(audHeader, audTimestamp);

			// Write audio frame
			ASSERT_TRUE(m_publisher->writeAudioFrame(
				audTimestamp, audHeader, audData));
		}
		m_publisher->endForceBufferWrite();

		// We want to pretend we are recording the video in real time but need
		// to make sure that the OS is ready for the data
		if(!m_publisher->willWriteBuffer()) {
#pragma warning(push)
#pragma warning(disable: 4996)
			_sleep(1000 / TEST_VIDEO_FRAME_RATE);
#pragma warning(pop)
		} else
			while(m_publisher->willWriteBuffer())
				m_errorSpy->wait(100); // HACK: Sleep while processing Qt

		// Write video frame
		m_publisher->beginForceBufferWrite();
		if(timestamp == 0) {
			// First IDR should be prefixed by an SEI
			QVector<QByteArray> pkts;
			pkts.append(QByteArray::fromRawData(
				reinterpret_cast<const char *>(testVidFrame0SEI),
				sizeof(testVidFrame0SEI)));
			pkts.append(data);
			ASSERT_TRUE(
				m_publisher->writeVideoFrame(timestamp, header, pkts));
		} else {
			QVector<QByteArray> pkts;
			pkts.append(data);
			ASSERT_TRUE(
				m_publisher->writeVideoFrame(timestamp, header, pkts));
		}
	}
	m_publisher->endForceBufferWrite();

	// Make sure that we didn't have any errors
	if(m_errorSpy->isEmpty())
		m_errorSpy->wait(500);
	ASSERT_TRUE(m_errorSpy->isEmpty());

	deletePublisher();
	disconnect();
}
#endif // DO_VIDEO_AUDIO_TEST_STREAM
