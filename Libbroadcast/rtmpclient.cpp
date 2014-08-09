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

#include "include/rtmpclient.h"
#include "include/amf.h"
#include "include/brolog.h"
#include "include/libbroadcast.h"
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#ifdef Q_OS_WIN
#include <WinSock2.h>
#endif

const QString LOG_CAT = QStringLiteral("RTMP");

#define DEBUG_LOW_LEVEL_RTMP 0
#define DEBUG_RTMP_HANDSHAKE 0

//=============================================================================
// Helpers

inline float fltLerp(float a, float b, float t)
{
	return a + t * (b - a);
}

static QString numberToHexString(quint64 num)
{
	return QStringLiteral("0x") + QString::number(num, 16).toUpper();
}

QString getSocketErrorString(QAbstractSocket::SocketError error)
{
	switch(error) {
	case QAbstractSocket::ConnectionRefusedError:
		return QStringLiteral("Connection refused");
	case QAbstractSocket::RemoteHostClosedError:
		return QStringLiteral("Remote host closed connection");
	case QAbstractSocket::HostNotFoundError:
		return QStringLiteral("Host address not found");
	case QAbstractSocket::SocketAccessError:
		return QStringLiteral("Application lacks required privileges");
	case QAbstractSocket::SocketResourceError:
		return QStringLiteral("Ran out of resources");
	case QAbstractSocket::SocketTimeoutError:
		return QStringLiteral("Timed out");
	case QAbstractSocket::DatagramTooLargeError:
		return QStringLiteral("Datagram larger than the OS's limit");
	case QAbstractSocket::NetworkError:
		return QStringLiteral("Network error");
	case QAbstractSocket::AddressInUseError:
		return QStringLiteral("Address already in use");
	case QAbstractSocket::SocketAddressNotAvailableError:
		return QStringLiteral("Address does not belong to the host");
	case QAbstractSocket::UnsupportedSocketOperationError:
		return QStringLiteral("Unsupported socket operation");
	case QAbstractSocket::ProxyAuthenticationRequiredError:
		return QStringLiteral("Proxy requires authentication");
	case QAbstractSocket::SslHandshakeFailedError:
		return QStringLiteral("SSL/TLS handshake failed");
	case QAbstractSocket::UnfinishedSocketOperationError:
		return QStringLiteral("Operation still in progress");
	case QAbstractSocket::ProxyConnectionRefusedError:
		return QStringLiteral("Proxy server connection refused");
	case QAbstractSocket::ProxyConnectionClosedError:
		return QStringLiteral("Proxy server connection closed");
	case QAbstractSocket::ProxyConnectionTimeoutError:
		return QStringLiteral("Proxy server connection timed out");
	case QAbstractSocket::ProxyNotFoundError:
		return QStringLiteral("Proxy address not found");
	case QAbstractSocket::ProxyProtocolError:
		return QStringLiteral("Proxy protocol error");
	case QAbstractSocket::OperationError:
		return QStringLiteral("Invalid socket operation");
	case QAbstractSocket::SslInternalError:
		return QStringLiteral("Internal SSL error");
	case QAbstractSocket::SslInvalidUserDataError:
		return QStringLiteral("Invalid SSL user data");
	case QAbstractSocket::TemporaryError:
		return QStringLiteral("Temporary error");
	case QAbstractSocket::UnknownSocketError:
		return QStringLiteral("Unknown socket error");
	default:
		return numberToHexString((uint)error);
	}
}

quint32 getCurrentTime32()
{
	return QDateTime::currentMSecsSinceEpoch() % UINT_MAX;
}

/// <summary>
/// QDataStream doesn't support 24-bit integers.
/// </summary>
void writeUInt24(QDataStream *stream, uint val)
{
	char data[3];
	amfEncodeUInt24(data, val);
	stream->writeRawData(data, 3);
}

/// <summary>
/// Writes a 32-bit little-endian unsigned integer to the specified
/// QDataStream. The only little-endian field in all of RTMP is the message
/// stream ID of a chunk type 0 header.
/// </summary>
void writeLEUInt32(QDataStream *stream, uint val)
{
	QDataStream::ByteOrder order = stream->byteOrder();
	stream->setByteOrder(QDataStream::LittleEndian);
	*stream << (quint32)val;
	stream->setByteOrder(order);
}

/// <summary>
/// Decodes a little-endian 32-bit unsigned integer. The only little-endian
/// field in all of RTMP is the message stream ID of a chunk type 0 header.
/// </summary>
uint decodeLEUInt32(const char *data)
{
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
#error Unsupported byte order
#endif
	return (uint)(*((quint32 *)data));
}

//=============================================================================
// RTMP Notes
/*

===== FMLE <-> Twitch network analysis =====

Sender	ChStrm	Strm	Desc
----------------------------------------------
... TCP/SSL initialized ...
Clnt	-		-		Handshake C0
Clnt	-		-		Handshake C1
Srvr	-		-		Handshake S0
Srvr	-		-		Handshake S1
Srvr	-		-		Handshake S2
Clnt	-		-		Handshake C2
Clnt	3		0		"connect({app='<app>', flashVer=..., ...})" (CommandMsg)
Srvr	2		0		WindowAckSize(2500000)
Srvr	2		0		SetPeerBandwidth(2500000, DYNAMIC)
Srvr	2		0		StreamBegin(0)
Srvr	2		0		SetChunkSize(4096)
Srvr	3		0		"_result({fmsVer=..., ...}, {...})" (CommandMsg)
Clnt	2		0		WindowAckSize(2500000)
... Initialized and ready to stream ...
Clnt	3		0		"releaseStream('<key>')" (CommandMsg)
Clnt	3		0		"FCPublish('<key>')" (CommandMsg)
Clnt	3		0		"createStream()" (CommandMsg)
Srvr	3		0		"onFCPublish({...})" (CommandMsg)
Srvr	3		0		"_result(1)" (CommandMsg)
Clnt	4		1		"publish('<key>', 'live')" (CommandMsg)
Srvr	2		0		StreamBegin(1)
Srvr	3		1		"onStatus({...})" (CommandMsg)
Clnt	4		1		"@setDataFrame('onMetaData', {...})" (DataMsg)
... Ready for video and audio data ...
Clnt	4		1		VideoData(...)
Clnt	4		1		VideoData(...)
Clnt	2		0		SetChunkSize(314)
Clnt	4		1		AudioData(...)
Clnt	4		1		AudioData(...)
... Video and audio data ...
Srvr	2		0		Ack(...)
... Video and audio data ...
Clnt	3		0		"FCUnpublish('<key>')" (CommandMsg)
Clnt	4		1		"closeStream()" (CommandMsg)
Srvr	3		0		"onFCUnpublish({...})" (CommandMsg)
Srvr	3		1		"onStatus({...})" (CommandMsg)
Clnt	3		0		"deleteStream(1)" (CommandMsg)
... TCP RST ...

Notes:
- FMLE and XSplit (By default) interweaves video and audio on the same chunk
stream while OBS has separate chunk streams.
- OBS sets the C->S chunk size before the "connect()" command.
- nginx-rtmp doesn't transmit S->C "StreamBegin" messages at all by default
therefore if the client receives a "_result()" then it assumes that it is also
a "StreamBegin" message if one hasn't already been received.

*/
//=============================================================================
// RTMPPublisher class

RTMPPublisher::RTMPPublisher(RTMPClient *client)
	: QObject()
	, m_client(client)
	, m_isReady(false)
	, m_isAvc(false)
{
}

RTMPPublisher::~RTMPPublisher()
{
}

void RTMPPublisher::setReady(bool isReady)
{
	if(m_isReady == isReady)
		return; // No change
	m_isReady = isReady;
	if(isReady) {
		emit ready();
		// TODO: Emit data request?
	}
}

/// <summary>
/// Begins the creation of the publishing stream. The application should
/// connect to the `ready()` and `socketDataRequest()` signals before calling
/// this method.
/// </summary>
/// <returns>True if the stream creation process has begun</returns>
bool RTMPPublisher::beginPublishing()
{
	// TODO: Prevent multiple calls
	return m_client->writeCreateStreamMsg();
}

bool RTMPPublisher::finishPublishing()
{
	if(!m_isReady)
		return false;
	return m_client->writeDeleteStreamMsg(0); // Autodetect stream ID
}

/// <summary>
/// Force all calls to `write()` to be buffered until `endForceBufferWrite()`
/// is called. This is required to prevent transmitting many small packets over
/// the network.
/// </summary>
void RTMPPublisher::beginForceBufferWrite()
{
	m_client->beginForceBufferWrite();
}

/// <summary>
/// Ends forced buffer mode of writes and flushes the write buffer.
/// </summary>
void RTMPPublisher::endForceBufferWrite()
{
	m_client->endForceBufferWrite();
}

/// <summary>
/// Will the next call to a write method buffer the data internally or write it
/// to the OS? Use this to more efficiently drop frames.
/// </summary>
/// <returns>True if the data will be buffered internally</returns>
bool RTMPPublisher::willWriteBuffer() const
{
	return m_client->willWriteBuffer();
}

/// <summary>
/// Writes the "@setDataFrame" message to the output buffer. This should be
/// called before any video or audio frames are written.
/// </summary>
/// <returns>True if the message was added to the output buffer</returns>
bool RTMPPublisher::writeDataFrame(AMFObject *data)
{
	if(!m_isReady)
		return false;
	return m_client->writeSetDataFrameMsg(data);
}

/// <summary>
/// Writes the "AVCDecoderConfigurationRecord" as specified in section 5.2.4.1
/// of ISO 14496-15:2004 to the output buffer. This should be written before
/// any H.264 video frames are written otherwise some decoders such as Flash
/// will not be able to parse the video stream.
/// </summary>
/// <returns>True if the record has added to the output buffer</returns>
bool RTMPPublisher::writeAvcConfigRecord(
	const QByteArray &sps, const QByteArray &pps)
{
	if(!m_isReady)
		return false;
	if(sps.isEmpty() || pps.isEmpty())
		return false;
	m_isAvc = true;

	// Create FLV "VideoTagHeader" structure
	char header[5];
	header[0] = 0x17; // AVC keyframe
	header[1] = 0x00; // AVC sequence header
	header[2] = 0x00; // Composition time always 0x000000
	header[3] = 0x00;
	header[4] = 0x00;
	QByteArray headerBuf = QByteArray::fromRawData(header, sizeof(header));

	// Find start of SPS NAL unit (Removes 0x00000001 if it exists)
	int spsOff = 0;
	while(spsOff < sps.size() - 1 && sps[spsOff] == 0)
		spsOff++;
	if(spsOff > 0 && sps[spsOff] == 0x01)
		spsOff++;

	// Find start of PPS NAL unit (Removes 0x00000001 if it exists)
	int ppsOff = 0;
	while(ppsOff < pps.size() - 1 && pps[ppsOff] == 0)
		ppsOff++;
	if(ppsOff > 0 && pps[ppsOff] == 0x01)
		ppsOff++;

	//-------------------------------------------------------------------------
	// Write record

	QByteArray data;
	data.reserve(32);

	if(sps.size() <= spsOff + 3)
		return false; // Prevent crashes
	data += 0x01; // "configurationVersion"
	data += sps[spsOff+1]; // "AVCProfileIndication"
	data += sps[spsOff+2]; // "profile_compatibility"
	data += sps[spsOff+3]; // "AVCLevelIndication"

	// We use 32-bit lengths (4 bytes) in our "AVCSample" structure
	data += (char)(0xFC | (4 - 1)); // "lengthSizeMinusOne" with reserved bits set

	// Write SPS to record including the H.264 "nal_unit" header byte
	data += (char)(0xE0 | 1); // "numOfSequenceParameterSets" with reserved bits set
	data += QByteArray(2, 0x00); // Reserve space for encode
	amfEncodeUInt16(&(data.data()[data.size()-2]), sps.size() - spsOff);
	data += sps.right(sps.size() - spsOff);

	// Write PPS to record including the H.264 "nal_unit" header byte
	data += 1; // "numOfPictureParameterSets"
	data += QByteArray(2, 0x00); // Reserve space for encode
	amfEncodeUInt16(&(data.data()[data.size()-2]), pps.size() - ppsOff);
	data += pps.right(pps.size() - ppsOff);

	return m_client->writeVideoData(0, headerBuf + data);
}

/// <summary>
/// Writes the AAC sequence header to the output buffer.
/// </summary>
/// <returns>True if the header has added to the output buffer</returns>
bool RTMPPublisher::writeAacSequenceHeader(const QByteArray &oob)
{
	if(!m_isReady)
		return false;
	QByteArray header;
	header.reserve(2);
	header += (char)0xAF; // AAC format (Constant)
	header += (char)0x00; // 0 = AAC sequence header, 1 = AAC data
	return m_client->writeAudioData(0, header + oob);
}

/// <summary>
/// Writes a single video frame to the output buffer. RTMP requires all frames
/// to be prefixed with the FLV "VideoTagHeader" structure which can be found
/// in section E.4.3.1 of the FLV and F4V specifications (v10.1).
///
/// This method will automatically wrap H.264 in a valid "AVCSample" structure
/// as specified in section 5.3.4.2 of ISO 14496-15:2004. All NAL units must be
/// grouped as specified by section 5.2.2 of the same specification.
///
/// When the FLV specification refers to a "composition time offset" it means
/// the difference between the PTS and DTS in milliseconds, i.e.
/// `CompositionTime = ((PTS - DTS) * TimeBase.num * 1000) / TimeBase.denom`.
/// For an explanation of composition times see section 8.6.1.1 and 8.6.1.3 of
/// ISO 14496-12:2008. The FLV specifications mention section 8.15.3 of the
/// second (2005) edition which has since been moved to section 8.6.1.3 in the
/// third (2008) edition.
/// </summary>
/// <returns>True if the video frame was added to the output buffer</returns>
bool RTMPPublisher::writeVideoFrame(
	quint32 timestamp, const QByteArray &header, QVector<QByteArray> pkts)
{
	if(!m_isReady)
		return false;
	QByteArray frameData;
	if(m_isAvc) {
		// Wrap H.264 in a "AVCSample" structure

		frameData = header;

		// Allocate memory all at once (Guessing maximum size)
		int dataSize = frameData.size();
		for(int i = 0; i < pkts.size(); i++)
			dataSize += pkts.at(i).size() + 4;
		frameData.reserve(dataSize);

		for(int i = 0; i < pkts.size(); i++) {
			const QByteArray &data = pkts.at(i);

			// Find start of NAL unit (Removes 0x00000001 if it exists)
			int off = 0;
			while(off < data.size() - 1 && data[off] == 0)
				off++;
			if(off > 0 && data[off] == 0x01)
				off++;

			// Convert data to a raw NAL unit
			QByteArray nal = data.right(data.size() - off);

			// Generate "AVCSample" header which is just a 32-bit length field
			char sample[4];
			amfEncodeUInt32(sample, nal.size());
			QByteArray sampleBuf =
				QByteArray::fromRawData(sample, sizeof(sample));

			// Append to frame data
			frameData += sampleBuf + nal;
		}
	} else {
		frameData = header;

		// Allocate memory all at once
		int dataSize = frameData.size();
		for(int i = 0; i < pkts.size(); i++)
			dataSize += pkts.at(i).size();
		frameData.reserve(dataSize);

		for(int i = 0; i < pkts.size(); i++)
			frameData += pkts.at(i);
	}
	return m_client->writeVideoData(timestamp, frameData);
}

/// <summary>
/// Writes a single audio frame to the output buffer. RTMP requires all frames
/// to be prefixed with the FLV "AudioTagHeader" structure which can be found
/// in section E.4.2.1 of the FLV and F4V specifications (v10.1).
/// </summary>
/// <returns>True if the audio frame was added to the output buffer</returns>
bool RTMPPublisher::writeAudioFrame(
	quint32 timestamp, const QByteArray &header, const QByteArray &data)
{
	if(!m_isReady)
		return false;
	return m_client->writeAudioData(timestamp, header + data);
}

//=============================================================================
// RTMPClient class

bool RTMPClient::s_inGamerMode = false;
float RTMPClient::s_gamerTickFreq = 1.0f;

QString RTMPClient::errorToString(RTMPClient::RTMPError error)
{
	switch(error) {
	case RTMPClient::UnknownError:
		return QStringLiteral("Unknown error");
	case RTMPClient::ConnectionRefusedError:
		return QStringLiteral("Connection refused");
	case RTMPClient::RemoteHostClosedError:
		return QStringLiteral("Remote host closed connection");
	case RTMPClient::HostNotFoundError:
		return QStringLiteral("Host address not found");
	case RTMPClient::TimeoutError:
		return QStringLiteral("Timed out");
	case RTMPClient::NetworkError:
		return QStringLiteral("Network error");
	case RTMPClient::SslHandshakeFailedError:
		return QStringLiteral("SSL/TLS handshake failed");
	case RTMPClient::UnexpectedResponseError:
		return QStringLiteral("Unexpected response");
	case RTMPClient::InvalidWriteError:
		return QStringLiteral("Invalid write");
	case RTMPClient::RtmpConnectRejectedError:
		return QStringLiteral("RTMP application connection rejected");
	case RTMPClient::RtmpCreateStreamError:
		return QStringLiteral("RTMP stream creation failed");
	case RTMPClient::RtmpPublishRejectedError:
		return QStringLiteral("Server rejected publish");
	default:
		return numberToHexString((uint)error);
	}
}

/// <summary>
/// Used to enable or disable "gamer mode" which reduces network interference
/// at the expense of increased maintenance and slightly slower responsiveness
/// to network congestion. Gamer mode is disabled by default. This setting must
/// only be changed when there are no active clients. If the OS output buffer
/// fills then gamer mode is temporarily disabled automatically as it can't
/// handle congestion properly and will have no effect anyway on a saturated
/// network.
/// </summary>
void RTMPClient::gamerModeSetEnabled(bool enabled)
{
	s_inGamerMode = enabled;
}

/// <summary>
/// Used to change the expected frequency of `gamerTickEvent()` calls. If the
/// method isn't called that many times per second (Or if all dropped ticks are
/// not accounted for) then the behaviour is undefined. This setting must only
/// be changed when there are no active clients.
/// </summary>
void RTMPClient::gamerSetTickFreq(float freq)
{
	s_gamerTickFreq = freq;
}

RTMPClient::RTMPClient()
	: QObject()
	, m_remoteInfo()
	, m_socket(this)
	, m_socketWriteNotifier(NULL)
	, m_autoInitialize(true)
	, m_autoAppConnect(true)
	, m_versionString(QStringLiteral("FMLE/3.0 (compatible; FMSc/1.0)"))
	, m_publisher(NULL)

	// Connection state
	, m_handshakeState(DisconnectedState)
	, m_handshakeRandomData()
	// All other members are initialized in `resetStateMembers()`

	// Input/output buffers
	, m_outBuf()
	, m_bufferOutBufRef(0)
	, m_writeStreamBuf(this)
	, m_writeStream()
	, m_inBuf()

	// Gamer mode
	, m_gamerOutBuf()
	, m_gamerAvgUploadBytes(100 * 1024 * 1024) // 100 MB/s
	, m_gamerInSatMode(false)
	, m_gamerSatModeTimer(0.0f)
	, m_gamerExitSatModeTime(10.0f)
{
	resetStateMembers();

	// Connect socket signals
	void (QAbstractSocket:: *fpseAS)(QAbstractSocket::SocketError) =
		&QAbstractSocket::error;
	QObject::connect(&m_socket, &QAbstractSocket::connected,
		this, &RTMPClient::socketConnected);
	QObject::connect(&m_socket, &QAbstractSocket::disconnected,
		this, &RTMPClient::socketDisconnected);
	QObject::connect(&m_socket, fpseAS,
		this, &RTMPClient::socketError);
	QObject::connect(&m_socket, &QIODevice::readyRead,
		this, &RTMPClient::socketDataReady);
}

void RTMPClient::resetStateMembers()
{
	if(m_publisher != NULL) {
		delete m_publisher;
		m_publisher = NULL;
	}

	m_inMaxChunkSize = 128;
	m_outMaxChunkSize = 128;
	m_inAckWinSize = 2500000; // Guessed default from librtmp
	m_outAckWinSize = 2500000; // Guessed default from librtmp
	m_inAckLimitType = HardLimitType; // Guessed default from FMLE
	m_inBytesSinceLastAck = 0;
	m_outBytesSinceLastAck = 0;
	m_inBytesSinceHandshake = 0;
	m_inChunkStreams.clear();
	m_outChunkStreams.clear();
	m_nextTransactionIds.clear();
	m_appConnected = false;
	m_appConnectTransId = 0;
	m_creatingStream = false;
	m_createStreamTransId = 0;
	m_publishStreamId = 0;
	m_beginningPublish = false;
	m_lastPublishTimestamp = 0;
}

RTMPClient::~RTMPClient()
{
	if(m_publisher != NULL) {
		delete m_publisher;
		m_publisher = NULL;
	}
	if(m_socketWriteNotifier != NULL) {
		delete m_socketWriteNotifier;
		m_socketWriteNotifier = NULL;
	}

	// Clean up the write stream if it hasn't completed yet
	if(m_writeStream.device() != NULL)
		endWriteStream(false);

	// Disconnect immediately if needed (Will be unclean)
	disconnect(false);
}

/// <summary>
/// Configures the client to connect to the specified remote host.
/// </summary>
/// <returns>True if the target is valid</returns>
bool RTMPClient::setRemoteTarget(const RTMPTargetInfo &info)
{
	// TODO: Validate input
	m_remoteInfo = info;
	return true;
}

/// <summary>
/// Configures the client to connect to the specified remote host using a URL.
/// </summary>
/// <returns>True if the URL is valid</returns>
bool RTMPClient::setRemoteTarget(const QString &url)
{
	// TODO: Unimplemented
	return false;
}

/// <summary>
/// Returns true if the network socket is connected to the remote host.
/// </summary>
bool RTMPClient::isSocketConnected() const
{
	switch(m_handshakeState) {
	default:
	case DisconnectedState:
	case ConnectingState:
		return false;
	case ConnectedState:
	case VersionSentState:
	case VersionReceivedState:
	case AckSentState:
	case InitializedState:
	case DisconnectingState: // Still technically connected
		return true;
	}
	// Should never happen
	return false;
}

/// <summary>
/// Begins the RTMP connection process. Signals will be emitted at specific
/// events to notify the caller when the connection is ready for use or if an
/// error occurred.
/// </summary>
/// <returns>True if the connection process was started</returns>
bool RTMPClient::connect()
{
	if(m_handshakeState != DisconnectedState)
		return false; // Already connected or connecting
	// TODO: Test for invalid host/port
	m_handshakeState = ConnectingState;
	m_outBuf.clear();
	m_inBuf.clear();
	m_gamerOutBuf.clear();
	m_gamerInSatMode = false;
	emit connecting();

	// We use an unbuffered socket so we can control the maximum amount of data
	// that can be pending for write. This is required to do more efficient
	// frame dropping.
	m_socket.connectToHost(m_remoteInfo.host, m_remoteInfo.port,
		QIODevice::ReadWrite | QIODevice::Unbuffered);

	return true;
}

/// <summary>
/// Begin the RTMP handshake process.
/// </summary>
/// <returns>True if the handshake process was started</returns>
bool RTMPClient::initialize()
{
	// From RTMP specification:
	// "The handshake begins with the client sending the C0 and C1 chunks."
	if(!writeC0S0())
		return false;
	if(!writeC1S1())
		return false;
	m_handshakeState = VersionSentState;
	return true;
}

/// <summary>
/// Begin the RTMP application connection process ("connect()" command).
/// </summary>
/// <returns>True if the connection process was started</returns>
bool RTMPClient::connectToApp()
{
	beginForceBufferWrite();
	if(!setMaxChunkSize(4096)) {
		endForceBufferWrite();
		return false;
	}
	m_appConnectTransId = getNextTransactionId(0);
	bool ret = writeConnectMsg(m_appConnectTransId);
	endForceBufferWrite();
	return ret;
}

/// <summary>
/// Marks this client as a publisher and prepares the `RTMPPublisher` object.
/// The actual RTMP stream is not created until the application calls
/// `RTMPPublisher::beginPublishing()`.
/// </summary>
RTMPPublisher *RTMPClient::createPublishStream()
{
	if(m_publisher == NULL)
		m_publisher = new RTMPPublisher(this);
	return m_publisher;
}

void RTMPClient::deletePublishStream()
{
	if(m_publisher == NULL)
		return;
	delete m_publisher;
	m_publisher = NULL;
}

void RTMPClient::disconnect(bool cleanDisconnect)
{
	if(m_handshakeState == DisconnectedState)
		return; // Already disconnected

	// If we're still in forced buffer mode then something is probably wrong
	if(m_bufferOutBufRef > 0) {
		broLog(LOG_CAT, BroLog::Warning)
			<< QStringLiteral("Attempting to disconnect while still in forced buffer mode");
		while(m_bufferOutBufRef > 0)
			endForceBufferWrite();
	}

	if(m_publisher != NULL) {
		delete m_publisher;
		m_publisher = NULL;
	}
	if(m_socketWriteNotifier != NULL) {
		delete m_socketWriteNotifier;
		m_socketWriteNotifier = NULL;
	}

	if(!cleanDisconnect) {
		// Disconnect uncleanly by closing the socket immediately
		m_socket.abort();
		m_handshakeState = DisconnectedState;
		m_outBuf.clear();
		m_inBuf.clear();
		m_gamerOutBuf.clear();
		emit disconnected();
		return;
	}

	// TODO: Write RTMP NetConnection "close()" message here?

	// Push all buffered data to Qt
	m_socket.write(m_outBuf);
	m_outBuf.clear();
	if(s_inGamerMode) {
		m_socket.write(m_gamerOutBuf);
		m_gamerOutBuf.clear();
	}

	// Disconnect cleanly taking into account that we might not have even fully
	// connected yet
	m_handshakeState = DisconnectingState;
	m_socket.disconnectFromHost();
	m_inBuf.clear();
	if(m_handshakeState == DisconnectingState &&
		m_socket.state() == QAbstractSocket::UnconnectedState)
	{
		// `socketDisconnected()` was never called but the socket is actually
		// disconnected
		m_handshakeState = DisconnectedState;
		emit disconnected();
	}
}

/// <summary>
/// Returns the size of the OS's TCP socket write buffer (`SO_SNDBUF`) or -1 on
/// failure. This may not match what was set with `setOSWriteBufferSize()` as
/// the OS can use a larger buffer size than what was set if it wants to.
/// </summary>
int RTMPClient::getOSWriteBufferSize() const
{
#ifdef Q_OS_WIN
	SOCKET desc = m_socket.socketDescriptor();
	int size = 0;
	int len = sizeof(size);
	int ret = getsockopt(desc, SOL_SOCKET, SO_SNDBUF, (char *)&size, &len);
	if(ret == 0)
		return size;
	//WSAGetLastError(); // TODO: How to return error?
#else
#error Unsupported platform
#endif
	return -1;
}

/// <summary>
/// Sets the size of the OS's TCP socket write buffer (`SO_SNDBUF`). Returns 0
/// on success or an error code on failure. This is used to limit the amount of
/// pending data to be transmitted so we can handle frame dropping better.
/// </summary>
int RTMPClient::setOSWriteBufferSize(int size)
{
#ifdef Q_OS_WIN
	SOCKET desc = m_socket.socketDescriptor();
	int ret =
		setsockopt(desc, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
	if(ret != 0)
		return WSAGetLastError();
#else
#error Unsupported platform
#endif
	return 0;
}

/// <summary>
/// Appends the specified data to the output buffer that will be transmitted
/// sometime in the future.
///
/// WARNING: This is a low-level method and should only be used for testing
/// purposes.
/// </summary>
/// <returns>True if the data was added to the buffer</returns>
bool RTMPClient::write(const QByteArray &data)
{
	// We can only write if we have a connected socket
	switch(m_handshakeState) {
	case DisconnectedState:
	case ConnectingState:
	case DisconnectingState:
		emit error(InvalidWriteError);
		return false;
	default:
		break;
	}

	// Fast exit if there is no data to write
	if(data.isEmpty())
		return true;

	// If we're in gamer mode then we buffer writes so we can write it once per
	// tick unless we're in "saturation mode" which we then behave normally.
	if(s_inGamerMode && !m_gamerInSatMode) {
		m_gamerOutBuf.append(data);
		return true;
	}

	// If we have been forced to buffer all writes then do so and return
	if(m_bufferOutBufRef > 0) {
		m_outBuf.append(data);
		return true;
	}

	// Write to the socket taking into account our internal buffer
	int ret = socketWrite(data);
	if(ret < 0)
		return false;
	return true;
}

/// <summary>
/// Writes the specified data to the Qt socket in a way that doesn't overflow
/// the OS buffer. Always use this instead of `m_socket.write()`. Will attempt
/// to write the internal buffer before the specified data if it's not empty.
/// Pass an empty `data` to attempt to flush the internal buffer only. If
/// `emitDataRequest` is true then if the buffer is fully emptied by this call
/// the class will request any listening publishers to write more data to the
/// socket.
/// </summary>
/// <returns>
/// The best guess of the number of free bytes in the OS's send buffer or -1 if
/// there was a socket error.
/// </returns>
int RTMPClient::socketWrite(const QByteArray &data, bool emitDataRequest)
{
	// TODO: Currently this method doesn't support emitting a data request at
	// the same time as writing more data to the socket. This is non-trivial to
	// add and may require partial rewriting.
	Q_ASSERT(!(emitDataRequest && !data.isEmpty()));

	if(isSocketConnected() &&
		(m_socket.state() == QAbstractSocket::UnconnectedState ||
		m_socket.socketDescriptor() == -1))
	{
		// The socket was disconnected but Qt never emitted a `disconnect()` or
		// `error()` signal. What most likely happened is that the remote host
		// closed the connection gracefully. We queue this signal for later as
		// it's not safe for us to issue a disconnect while in this method.
		broLog(LOG_CAT, BroLog::Warning)
			<< QStringLiteral("Attempted to write to closed socket");
		QTimer::singleShot(0, this, SLOT(socketRemoteDisconnectTimeout()));
		return -1;
	}

	int osWriteBufSize = getOSWriteBufferSize();
	if(osWriteBufSize < 0) {
		// Socket isn't valid. We were most likely disconnected without knowing
		// about it. Should never happen as we checked if the socket
		// disconnected above
		return -1;
	}

	// If there is anything in Qt's buffer attempt to flush it. If it cannot be
	// flushed then we know that the OS buffer is full.
	if(m_socket.bytesToWrite() > 0) {
		// If we fully flush then we know that the OS buffer is partially
		// filled and we shouldn't attempt to write as much data later
		osWriteBufSize -= m_socket.bytesToWrite();
		osWriteBufSize = qMax(0, osWriteBufSize); // Done for safety
		m_socketWriteNotifier->setEnabled(false);
		m_socket.flush();
		if(m_socket.bytesToWrite() > 0) {
			//broLog() << "Waiting to empty Qt buffer: "
			//	<< m_socket.bytesToWrite();
			// OS buffer is full, buffer the new data for later
			m_outBuf.append(data);
			m_socketWriteNotifier->setEnabled(true);
			gamerEnterSatMode();
			return 0;
		}
	}

	// Attempt to flush any pending data in the buffer
	if(!m_outBuf.isEmpty()) {
		// Prevent just moving our internal buffer to Qt's buffer where we can
		// no longer track congestion.
		int bytesToWrite = qMin(m_outBuf.size(), osWriteBufSize);

		// Call Qt's `write()` method. In "unbuffered" mode Qt will still
		// buffer any data that was not written to the OS buffer so `written`
		// can only be used to detect writing errors.
		m_socketWriteNotifier->setEnabled(false);
		qint64 written = m_socket.write(m_outBuf, bytesToWrite);
		//broLog() << "Wrote: " << bytesToWrite;
		if(written < 0)
			return -1;
		Q_ASSERT(written == bytesToWrite); // Should be all or nothing

		// Remove written data from our internal buffer
		if(written >= m_outBuf.size()) {
			emit dataWritten(m_outBuf); // TODO: Remove if possible
			m_outBuf.clear();
		} else {
			emit dataWritten(m_outBuf.left(written)); // TODO: Remove if possible
			m_outBuf.remove(0, written);
			m_socketWriteNotifier->setEnabled(true);
			//gamerEnterSatMode(); // Dangerous as it can write to the buffer
		}

		// As we just wrote to the OS buffer we know that it's now partially
		// filled.
		osWriteBufSize = qMax(0, osWriteBufSize - (int)written);

		// Emit a data request if we now have an empty buffer. In gamer mode we
		// let the caller emit the request as gamer mode has its own separate
		// buffer.
		if(m_outBuf.isEmpty() && emitDataRequest) {
			//broLog() << "Buffer empty, emitting data request";
			if(!s_inGamerMode || m_gamerInSatMode) {
				int bytesLeft = qMax(1, osWriteBufSize); // Ensure >= 1
				if(m_publisher != NULL)
					m_publisher->socketDataRequest(bytesLeft); // Remote emit
			}
			return 0; // TODO?
		}

		// If there is anything in Qt's buffer or our buffer then we know that
		// the new data will never be written to the OS buffer. Buffer it and
		// return.
		if(m_socket.bytesToWrite() > 0 || !m_outBuf.isEmpty()) {
			m_outBuf.append(data);
			m_socketWriteNotifier->setEnabled(true);
			gamerEnterSatMode();
			//broLog() << "Waiting to empty buffers: Qt="
			//	<< m_socket.bytesToWrite() << ", internal=" << m_outBuf.size();
			return 0;
		}
	}

	if(data.isEmpty())
		return osWriteBufSize; // Nothing else to do

	// Prevent just moving our internal buffer to Qt's buffer where we can no
	// longer track congestion.
	int bytesToWrite = qMin(data.size(), osWriteBufSize);

	// Call Qt's `write()` method. In "unbuffered" mode Qt will still
	// buffer any data that was not written to the OS buffer so `written`
	// can only be used to detect writing errors.
	m_socketWriteNotifier->setEnabled(false);
	qint64 written = m_socket.write(data, bytesToWrite);
	//broLog() << "Wrote: " << bytesToWrite;
	if(written < 0)
		return -1;
	Q_ASSERT(written == bytesToWrite); // Should be all or nothing

	// As we just wrote to the OS buffer we know that it's now partially filled
	osWriteBufSize = qMax(0, osWriteBufSize - (int)written);

	if(written == data.size()) {
		//broLog() << "Wrote entire message";
		emit dataWritten(data); // TODO: Remove if possible
	} else {
		// We only partially wrote our data to the socket, buffer the rest
		//broLog() << "Only wrote " << written << " out of " << data.size();
		emit dataWritten(data.left(written)); // TODO: Remove if possible
		m_outBuf.append(data.right(data.size() - written));
		m_socketWriteNotifier->setEnabled(true);
		gamerEnterSatMode();
	}
	return osWriteBufSize;
}

/// <summary>
/// Force all calls to `write()` to be buffered until `endForceBufferWrite()`
/// is called. This is required to prevent transmitting many small packets over
/// the network.
/// </summary>
void RTMPClient::beginForceBufferWrite()
{
	m_bufferOutBufRef++;
}

/// <summary>
/// Ends forced buffer mode of writes and flushes the write buffer.
/// </summary>
void RTMPClient::endForceBufferWrite()
{
	if(m_bufferOutBufRef <= 0)
		return; // Already out of forced buffer mode
	m_bufferOutBufRef--;
	if(m_bufferOutBufRef == 0)
		attemptToEmptyOutBuf();
}

/// <summary>
/// Attempt to write the entire pending output buffer to the socket. If
/// `emitDataRequest` is true then if the buffer is fully emptied by this call
/// the class will request any listening publishers to write more data to the
/// socket.
/// </summary>
/// <returns>True if the output buffer is empty</returns>
bool RTMPClient::attemptToEmptyOutBuf(bool emitDataRequest)
{
	if(m_outBuf.isEmpty())
		return true; // Buffer is already empty
	if(s_inGamerMode && !m_gamerInSatMode)
		return m_outBuf.isEmpty();
	socketWrite(QByteArray(), emitDataRequest);
	return m_outBuf.isEmpty();
}

/// <summary>
/// Will the next call to `write()` buffer the data internally or write it to
/// the OS? Used by publishers so they can more efficiently drop frames.
/// </summary>
/// <returns>True if the data will be buffered internally</returns>
bool RTMPClient::willWriteBuffer() const
{
	// If we've forced buffering then it will definitely buffer
	if(m_bufferOutBufRef > 0)
		return true;
	// If we have anything in our internal buffer then it's most likely because
	// the OS's TCP write buffer is full.
	if(!m_outBuf.isEmpty())
		return true;
	return false;
}

/// <summary>
/// Creates a big-endian data stream that will be written to the output buffer
/// when `endWriteStream()` is called. Note that a call to `endWriteStream()`
/// is required before another call to `beginWriteStream()` is allowed.
///
/// WARNING: This is a low-level method and should only be used for testing
/// purposes.
/// </summary>
QDataStream *RTMPClient::beginWriteStream()
{
	if(m_writeStream.device() != NULL) {
		broLog(LOG_CAT, BroLog::Warning)
			<< QStringLiteral("Attempted to create a second write stream");
		emit error(InvalidWriteError);
		return &m_writeStream; // Assume it's safe to do this
	}

	// Truncate buffer
	if(m_writeStreamBuf.isOpen())
		m_writeStreamBuf.close();
	m_writeStreamBuf.setData(QByteArray());

	m_writeStreamBuf.open(QIODevice::WriteOnly);
	m_writeStream.setDevice(&m_writeStreamBuf);
	m_writeStream.setByteOrder(QDataStream::BigEndian);
	m_writeStream.setFloatingPointPrecision(QDataStream::SinglePrecision);
	m_writeStream.setVersion(12);
	return &m_writeStream;
}

/// <summary>
/// See `beginWriteStream()`. If `writeToBuffer` is false then the data is
/// discarded and not written to the output buffer.
///
/// WARNING: This is a low-level method and should only be used for testing
/// purposes.
/// </summary>
/// <returns>True if the data was added to the buffer</returns>
bool RTMPClient::endWriteStream(bool writeToBuffer)
{
	if(m_writeStream.device() == NULL)
		return false;
	if(!m_writeStreamBuf.isOpen())
		return false;
	m_writeStreamBuf.close();
	bool ret = true;
	if(writeToBuffer)
		ret = write(m_writeStreamBuf.data());
	m_writeStream.setDevice(NULL);
	return ret;
}

/// <summary>
/// Writes the C0/S0 handshake packet to the output buffer.
/// </summary>
/// <returns>True if the packet was added to the buffer</returns>
bool RTMPClient::writeC0S0()
{
	QDataStream *stream = beginWriteStream();
	*stream << (quint8)3; // "3" = RTMP v1.0
	bool ret = endWriteStream();
#if DEBUG_RTMP_HANDSHAKE
	if(ret)
		broLog(LOG_CAT) << "Wrote C0";
	else
		broLog(LOG_CAT, BroLog::Warning) << "Failed to write C0";
#endif
	return ret;
}

/// <summary>
/// Writes the C1/S1 handshake packet to the output buffer.
/// </summary>
/// <returns>True if the packet was added to the buffer</returns>
bool RTMPClient::writeC1S1()
{
	// Generate 1528 random bytes
	m_handshakeRandomData.resize(1528);
	for(int i = 0; i < m_handshakeRandomData.size(); i++)
		m_handshakeRandomData.data()[i] = (char)qrand();

	QDataStream *stream = beginWriteStream();
	*stream << getCurrentTime32();
	*stream << (quint32)0;
	stream->writeRawData(m_handshakeRandomData.constData(), 1528);
	bool ret = endWriteStream();
#if DEBUG_RTMP_HANDSHAKE
	if(ret)
		broLog(LOG_CAT) << "Wrote C1";
	else
		broLog(LOG_CAT, BroLog::Warning) << "Failed to write C1";
#endif
	return ret;
}

/// <summary>
/// Writes the C2/S2 handshake packet to the output buffer.
/// </summary>
/// <returns>True if the packet was added to the buffer</returns>
bool RTMPClient::writeC2S2(quint32 time, const QByteArray &echo)
{
	if(echo.size() != 1528)
		return false; // Invalid echo size
	QDataStream *stream = beginWriteStream();
	*stream << time;
	*stream << getCurrentTime32();
	stream->writeRawData(echo.constData(), 1528);
	bool ret = endWriteStream();
#if DEBUG_RTMP_HANDSHAKE
	if(ret)
		broLog(LOG_CAT) << "Wrote C2";
	else
		broLog(LOG_CAT, BroLog::Warning) << "Failed to write C2";
#endif
	return ret;
}

/// <summary>
/// Writes the specified RTMP message to the output buffer.
/// </summary>
/// <returns>True if the message was added to the buffer</returns>
bool RTMPClient::writeMessage(
	uint streamId,  RTMPClient::RTMPMsgType type, quint32 timestamp,
	const QByteArray &msg, uint csId)
{
	// Initialize output chunk stream state if it's a new chunk stream
	bool isNew = initOutChunkStreamState(csId);
	if(isNew) {
#if DEBUG_LOW_LEVEL_RTMP
		broLog(LOG_CAT)
			<< QStringLiteral("New output chunk stream ID: %L1").arg(csId);
#endif // DEBUG_LOW_LEVEL_RTMP
	}
	ChunkStreamState state = m_outChunkStreams[csId];

	// Validate input
	if(csId > 65599 || csId <= 1) {
		emit error(InvalidWriteError);
		return false;
	}

	// Determine which message header type we will use. We want to use the
	// smallest one possible.
	uint fmt = 3;
	// TODO: Make sure fmt=0: Delta = timestamp (Needed?)
	if(state.timestampDelta != timestamp - state.timestamp)
		fmt = 2;
	if(state.msgLen != msg.size() || state.msgType != type)
		fmt = 1;
	if(timestamp == 0 || timestamp < state.timestamp ||
		state.msgStreamId != streamId)
	{
		fmt = 0;
		if(timestamp < state.timestamp) {
			// Timestamps should never decrease. See:
			// http://www.wowza.com/forums/showthread.php?19539-RTMP-timestamp-values-on-live-streaming
			broLog(LOG_CAT, BroLog::Warning)
				<< QStringLiteral("Timestamp went back in time. Was=%L1, now=%L2")
				.arg(state.timestamp).arg(timestamp);
		}
	}

	// Split the message up into chunks while writing to the data stream
	QDataStream *stream = beginWriteStream();
	Q_ASSERT(state.msgLenRemaining == 0);
	state.msgLenRemaining = msg.size();
	do {
		// How much of the message can we send in this chunk?
		int chunkLen = qMin(state.msgLenRemaining, m_outMaxChunkSize);

		// Write basic header. We want to use the smallest one possible that
		// can represent the chunk stream ID
		int headSize = 0;
		if(csId <= 63) {
			// 1 byte basic header
			unsigned char uc[1];
			uc[0] = (fmt << 6) | csId;
			stream->writeRawData((char *)uc, 1);
			headSize += 1;
		} else if(csId <= 319) {
			// 2 byte basic header
			unsigned char uc[2];
			uc[0] = (fmt << 6);
			uc[1] = csId - 64;
			stream->writeRawData((char *)uc, 2);
			headSize += 2;
		} else {
			// 3 byte basic header
			unsigned char uc[3];
			uc[0] = (fmt << 6) | 1;
			uc[1] = ((csId - 64) >> 8);
			uc[2] = (csId - 64) & 0xff;
			stream->writeRawData((char *)uc, 3);
			headSize += 3;
		}

		// Write message header
		switch(fmt) {
		default: // It's impossible to get a result that's outside 0-3
		case 0:
			// Update state
			state.timestamp = timestamp;
			state.timestampDelta = timestamp; // Specification is weird
			state.msgLen = msg.size();
			state.msgType = type;
			state.msgStreamId = streamId;

			// Write header
			if(state.timestamp >= 0xFFFFFF)
				writeUInt24(stream, 0xFFFFFF);
			else
				writeUInt24(stream, state.timestamp);
			writeUInt24(stream, state.msgLen);
			*stream << (quint8)state.msgType;
			writeLEUInt32(stream, state.msgStreamId); // Little-endian
			headSize += 11;
			if(state.timestamp >= 0xFFFFFF) {
				// Write extended timestamp
				*stream << (quint32)state.timestamp;
				headSize += 4;
			}
			break;
		case 1:
			// Update state
			state.timestampDelta = timestamp - state.timestamp;
			state.timestamp = timestamp;
			state.msgLen = msg.size();
			state.msgType = type;

			// Write header
			writeUInt24(stream, state.timestampDelta);
			writeUInt24(stream, state.msgLen);
			*stream << (quint8)state.msgType;
			headSize += 7;
			break;
		case 2:
			// Update state
			state.timestampDelta = timestamp - state.timestamp;
			state.timestamp = timestamp;

			// Write header
			writeUInt24(stream, state.timestampDelta);
			headSize += 3;
			break;
		case 3:
			// No header
			break;
		}

		// Write chunk payload
		const char *data =
			&msg.constData()[msg.size() - state.msgLenRemaining];
		stream->writeRawData(data, chunkLen);
		state.msgLenRemaining -= chunkLen;

#if DEBUG_LOW_LEVEL_RTMP
		broLog(LOG_CAT)
			<< QStringLiteral(">>   Sent chunk type %1 of size %L2 to chunk stream %L3")
			.arg(fmt).arg(headSize + chunkLen).arg(csId);
#endif // DEBUG_LOW_LEVEL_RTMP

		// Do we need to send more chunks to completely write this message?
		if(state.msgLenRemaining > 0) {
			// We do. Change the message header format to "type 3". We assume
			// that the timestamp delta is only applied to "type 3" headers if
			// the current chunk isn't a continuation of the previous one. See
			// the comments in `readChunkFromSocket()` about header types.
			fmt = 3;
		}
	} while(state.msgLenRemaining > 0);

	// TODO: We need to monitor `m_outBytesSinceLastAck` and not write data to
	// the socket if the remote host hasn't acknowledged the previous window.

	// Write the chunks to the output buffer
	bool ret = endWriteStream();
	if(!ret)
		return false; // Failed to write

#if DEBUG_LOW_LEVEL_RTMP
	broLog(LOG_CAT)
		<< QStringLiteral(">>   Sent message type %L1 of size %L2 to stream %L3")
		.arg((uint)type).arg(msg.size()).arg(streamId);
#endif // DEBUG_LOW_LEVEL_RTMP

	// Remember state for next time
	m_outChunkStreams[csId] = state;

	return true;
}

/// <summary>
/// Acknowledge all data that has been received from the remote host.
/// </summary>
/// <returns>True if the acknowledge was added to the output buffer</returns>
bool RTMPClient::writeAcknowledge()
{
	char data[4];
	char *off = data;
	off = amfEncodeUInt32(off, m_inBytesSinceHandshake);
	return writeMessage(0, AckMsgType, 0,
		QByteArray::fromRawData(data, sizeof(data)), 2);
}

/// <summary>
/// Response to the PingRequest user control message.
/// </summary>
/// <returns>True if the response was added to the output buffer</returns>
bool RTMPClient::writePingResponse(uint timestamp)
{
	char data[6];
	char *off = data;
	off = amfEncodeUInt16(off, PingResponseType);
	off = amfEncodeUInt32(off, timestamp);
	return writeMessage(0, UserControlMsgType, 0,
		QByteArray::fromRawData(data, sizeof(data)), 2);
}

bool RTMPClient::writeVideoData(uint timestamp, const QByteArray &data)
{
	bool ret = writeMessage(
		m_publishStreamId, VideoMsgType, timestamp, data, 4);
	if(ret && timestamp > m_lastPublishTimestamp)
		m_lastPublishTimestamp = timestamp;
	return ret;
}

bool RTMPClient::writeAudioData(uint timestamp, const QByteArray &data)
{
	bool ret = writeMessage(
		m_publishStreamId, AudioMsgType, timestamp, data, 4);
	if(ret && timestamp > m_lastPublishTimestamp)
		m_lastPublishTimestamp = timestamp;
	return ret;
}

/// <summary>
/// Writes the AMF 0 "connect()" message to the output buffer.
/// </summary>
/// <returns>True if the message was added to the buffer</returns>
bool RTMPClient::writeConnectMsg(uint transactionId)
{
	if(m_appConnected)
		return false; // Already connected

	QByteArray data = AMFString("connect").serialized();
	data.append(AMFNumber(transactionId).serialized());

	// Behave exactly like FMLE
	AMFObject obj;
	if(!m_remoteInfo.appInstance.isEmpty()) {
		// Providers with application instances: Ustream
		obj["app"] = new AMFString(
			m_remoteInfo.appName + "/" + m_remoteInfo.appInstance);
	} else {
		// Providers without application instances: Twitch, Justin.tv
		obj["app"] = new AMFString(m_remoteInfo.appName);
	}
	obj["tcUrl"] = new AMFString(m_remoteInfo.asUrl());
	obj["type"] = new AMFString("nonprivate");
	obj["flashVer"] = new AMFString(m_versionString);
	obj["swfUrl"] = new AMFString(m_remoteInfo.asUrl());
	data.append(obj.serialized());

	return writeMessage(0, CommandAmf0MsgType, 0, data, 3);
}

/// <summary>
/// Writes the AMF 0 "createStream()" message to the output buffer. Includes
/// additional "releaseStream()" and "FCPublish()" messages for improved
/// compatibility if we're creating a "publish()" stream.
/// </summary>
/// <returns>True if the message was added to the buffer</returns>
bool RTMPClient::writeCreateStreamMsg()
{
	if(m_creatingStream)
		return false; // We can only create one stream at a time

	// FMLE sends "releaseStream()" and "FCPublish()" before "createStream()"
	// if it is for a stream that we will be calling "publish()" on
	beginForceBufferWrite();
	if(m_publisher != NULL) {
		// releaseStream()
		QByteArray data = AMFString("releaseStream").serialized();
		data.append(AMFNumber(getNextTransactionId(0)).serialized());
		data.append(AMFNull().serialized());
		data.append(AMFString(m_remoteInfo.streamName).serialized());
		if(!writeMessage(0, CommandAmf0MsgType, 0, data, 3)) {
			endForceBufferWrite();
			return false;
		}

		// FCPublish()
		data = AMFString("FCPublish").serialized();
		data.append(AMFNumber(getNextTransactionId(0)).serialized());
		data.append(AMFNull().serialized());
		data.append(AMFString(m_remoteInfo.streamName).serialized());
		if(!writeMessage(0, CommandAmf0MsgType, 0, data, 3)) {
			endForceBufferWrite();
			return false;
		}
	}

	// createStream()
	m_creatingStream = true;
	m_createStreamTransId = getNextTransactionId(0);
	QByteArray data = AMFString("createStream").serialized();
	data.append(AMFNumber(m_createStreamTransId).serialized());
	data.append(AMFNull().serialized());
	bool ret = writeMessage(0, CommandAmf0MsgType, 0, data, 3);
	endForceBufferWrite();
	if(ret)
		return true;
	m_creatingStream = false;
	m_createStreamTransId = 0;
	return false;
}

/// <summary>
/// Writes the AMF 0 "closeStream()" and "deleteStream()" messages to the
/// output buffer. Includes an additional "FCUnpublish()" messages for improved
/// compatibility if it was a "publish()" stream. As there is no standardised
/// behaviour for acknowledging stream deletions we assume it will always
/// succeed. If `streamId` is zero the method will automatically choose the
/// stream to delete (E.g. the publisher stream)
/// </summary>
/// <returns>True if the messages were added to the buffer</returns>
bool RTMPClient::writeDeleteStreamMsg(uint streamId)
{
	quint32 closeTimestamp = 0;
	if(streamId == 0) {
		if(m_publisher == NULL || m_publishStreamId == 0)
			return false;
		streamId = m_publishStreamId;
		closeTimestamp = m_lastPublishTimestamp;
	}

	beginForceBufferWrite();

	// FCUnpublish()
	if(streamId == m_publishStreamId) {
		QByteArray data = AMFString("FCUnpublish").serialized();
		data.append(AMFNumber(getNextTransactionId(0)).serialized());
		data.append(AMFNull().serialized());
		data.append(AMFString(m_remoteInfo.streamName).serialized());
		if(!writeMessage(0, CommandAmf0MsgType, 0, data, 3)) {
			endForceBufferWrite();
			return false;
		}
	}

	// closeStream()
	QByteArray data = AMFString("closeStream").serialized();
	data.append(AMFNumber(0).serialized());
	data.append(AMFNull().serialized());
	if(!writeMessage(streamId, CommandAmf0MsgType, closeTimestamp, data, 4)) {
		endForceBufferWrite();
		return false;
	}

	// deleteStream(). While FMLE sends no transaction ID librtmp does and it
	// makes more sense to include one so that's what we do.
	data = AMFString("deleteStream").serialized();
	data.append(AMFNumber(getNextTransactionId(0)).serialized());
	data.append(AMFNull().serialized());
	data.append(AMFNumber(streamId).serialized());
	bool ret = writeMessage(0, CommandAmf0MsgType, 0, data, 3);

	endForceBufferWrite();
	m_nextTransactionIds.remove(streamId);
	if(streamId == m_publishStreamId)
		m_publishStreamId = 0;

	return ret;
}

/// <summary>
/// Writes the AMF 0 "publish()" message to the output buffer.
/// </summary>
/// <returns>True if the message was added to the buffer</returns>
bool RTMPClient::writePublishMsg(uint streamId)
{
	m_beginningPublish = true;
	QByteArray data = AMFString("publish").serialized();
	data.append(AMFNumber(0).serialized()); // No transaction ID
	data.append(AMFNull().serialized());
	data.append(AMFString(m_remoteInfo.streamName).serialized());
	data.append(AMFString("live").serialized());
	bool ret = writeMessage(streamId, CommandAmf0MsgType, 0, data, 4);
	if(ret)
		return true;
	m_beginningPublish = false;
	return false;
}

/// <summary>
/// Writes the AMF 0 "@setDataFrame()" message to the output buffer.
/// </summary>
/// <returns>True if the message was added to the buffer</returns>
bool RTMPClient::writeSetDataFrameMsg(AMFObject *streamData)
{
	if(m_publisher == NULL || m_publishStreamId == 0)
		return false;
	QByteArray data = AMFString("@setDataFrame").serialized();
	data.append(AMFString("onMetaData").serialized());
	data.append(streamData->serialized());
	return writeMessage(m_publishStreamId, DataAmf0MsgType, 0, data, 4);
}

/// <summary>
/// Sets the maximum output chunk size.
/// </summary>
/// <returns>True if the message was added to the output buffer</returns>
bool RTMPClient::setMaxChunkSize(uint maxSize)
{
	char data[4];
	char *off = data;
	off = amfEncodeUInt32(off, maxSize & 0x7FFFFFFF);
	bool ret = writeMessage(0, SetChunkSizeMsgType, 0,
		QByteArray::fromRawData(data, sizeof(data)), 2);
	if(!ret)
		return false;
	m_outMaxChunkSize = maxSize;
	return true;
}

/// <summary>
/// Notify the remote host to send an acknowledgement every X bytes.
/// </summary>
/// <returns>True if the message was added to the output buffer</returns>
bool RTMPClient::setAckWinSize(uint ackWinSize)
{
	char data[4];
	char *off = data;
	off = amfEncodeUInt32(off, ackWinSize);
	bool ret = writeMessage(0, WindowAckSizeMsgType, 0,
		QByteArray::fromRawData(data, sizeof(data)), 2);
	if(!ret)
		return false;
	m_outAckWinSize = ackWinSize;
	return true;
}

/// <summary>
/// Request that the remote host limit its output bandwidth by setting its
/// acknowledgement window to the specified settings.
/// </summary>
/// <returns>True if the message was added to the output buffer</returns>
bool RTMPClient::setPeerBandwidth(uint ackWinSize, AckLimitType limitType)
{
	char data[5];
	char *off = data;
	off = amfEncodeUInt32(off, ackWinSize);
	off = amfEncodeUInt8(off, limitType);
	return writeMessage(0, SetPeerBWMsgType, 0,
		QByteArray::fromRawData(data, sizeof(data)), 2);
}

/// <summary>
/// Sets the approximate upload speed (In bytes per second) that gamer mode
/// will use to calculate how much it should throttle. The actual throttle
/// amount will be higher than what is set here in order to allow for error.
/// </summary>
void RTMPClient::gamerSetAverageUpload(int avgUploadBytes)
{
	// Minimum value is there just to make sure we get some sort of output if
	// something goes wrong
	m_gamerAvgUploadBytes = qMax(5 * 1024, avgUploadBytes);
}

/// <summary>
/// When in gamer mode this method MUST be called on EVERY active object once
/// per tick. Failure to do so will result in extremely poor network usage and
/// potentially misbehaving connections. `numDropped` is the number of ticks
/// that were missed since the last call to this method (Ideally 0).
/// </summary>
void RTMPClient::gamerTickEvent(int numDropped)
{
	if(!s_inGamerMode)
		return; // This method should only be called in gamer mode
	if(!isSocketConnected())
		return; // Nowhere to write
	if(willWriteBuffer())
		return; // The OS output buffer is full, we'll flush when it's ready

	// If we're in "saturation mode" then we need to monitor the output buffer
	// to know when we can exit the mode. Saturation mode gets entered whenever
	// the OS buffer is full which immediately flushes our gamer output buffer
	// and writes immediately get written to the main output buffer. We exit
	// saturation mode once the OS buffer hasn't been filled for a certain
	// amount of time.
	if(m_gamerInSatMode) {
		m_gamerSatModeTimer += (float)(1 + numDropped) / s_gamerTickFreq;
		if(m_gamerSatModeTimer >= m_gamerExitSatModeTime)
			gamerExitSatMode();
		else
			return; // Still in saturation mode
	}

	if(m_gamerOutBuf.isEmpty())
		return; // Nothing to write

	//-------------------------------------------------------------------------
	// Calculate the amount of bytes that can be uploaded right now. We
	// increase the throttle amount above the ideal average in order to make
	// sure that we can ALWAYS empty our buffer. Ideally the throttle should be
	// as low as possible in order to reduce interference with other
	// applications. The constants below have undergone extensive tuning so
	// should not be modified unless absolutely required.

	float numSecsInBuf =
		(float)m_gamerOutBuf.size() / (float)m_gamerAvgUploadBytes;

	// Static multiplier: Ideally this should be between 1.2x and 1.5x. It
	// seems that lower values causes instability as the bitrate nears the
	// maximum throughput of the network though. For example on an internet
	// connection that can just barely upload at 700 Kbps having a multiplier
	// of 1.5x results in proper operation but a multipler of 1.4x or lower
	// results in lag in other applications to appear like the network has
	// reached saturation even if it hasn't.
	const float GAMER_THROTTLE_MULTIPLY = 1.3f;

	// Dynamic multipler: 1.7x @ 0.2s to 1.3x @ 0.4s
	//const float GAMER_THROTTLE_MULTIPLY = fltLerp(1.7f, 1.3f,
	//	qBound(0.0f, (numSecsInBuf - 0.2f) * 5.0f, 1.0f));

	//broLog() << "Secs in buf: " << QString::number(numSecsInBuf, 'f', 3)
	//	<< "\tMultiply: " << GAMER_THROTTLE_MULTIPLY;
	float maxAvgBytes = ((float)m_gamerAvgUploadBytes / s_gamerTickFreq) *
		(float)(1 + numDropped);
	int maxBytes = (int)(maxAvgBytes * GAMER_THROTTLE_MULTIPLY);

	//-------------------------------------------------------------------------

	// Create our buffer of data to upload right now
	QByteArray bufToOut;
	if(maxBytes >= m_gamerOutBuf.size()) {
		bufToOut = m_gamerOutBuf;
		m_gamerOutBuf.clear();
	} else {
		bufToOut = m_gamerOutBuf.left(maxBytes);
		m_gamerOutBuf.remove(0, maxBytes);
	}

	// Debugging
	//broLog() << "Uploading " << bufToOut.size() << " B of a maximum of "
	//	<< maxBytes << " B this tick";

	// Actually write to the socket (Or the overflow buffer)
	socketWrite(bufToOut);
}

/// <summary>
/// Called whenever the OS begins to buffer our output while in gamer mode.
/// While in "saturation mode" we temporarily disable the custom output
/// algorithm as it behaves as an unmonitored buffer that negatively affects
/// our ability to monitor the network.
/// </summary>
void RTMPClient::gamerEnterSatMode()
{
	if(!s_inGamerMode)
		return; // This method should only be called in gamer mode

	// This method is called whenever we detect that the OS buffer is full.
	// Reset our timer so we don't prematurely exit saturation mode.
	m_gamerSatModeTimer = 0.0f;

	if(m_gamerInSatMode)
		return; // Already in saturation mode
	if(!isSocketConnected())
		return; // We must have a valid socket

	broLog(LOG_CAT, BroLog::Warning) << QStringLiteral(
		"Network congestion detected, entering saturation mode");

	// Enter saturation mode (Also prevents recursion with our flush)
	m_gamerInSatMode = true;

	// Enable Nagle's algorithm
	m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 0);

	// Flush our gamer output buffer to the main output buffer
	if(!m_gamerOutBuf.isEmpty()) {
		QByteArray tmpBuf = m_gamerOutBuf;
		m_gamerOutBuf.clear();
		socketWrite(tmpBuf);
	}
}

void RTMPClient::gamerExitSatMode()
{
	if(!s_inGamerMode)
		return; // This method should only be called in gamer mode
	if(!m_gamerInSatMode)
		return; // Already out of saturation mode
	if(!isSocketConnected())
		return; // We must have a valid socket

	broLog(LOG_CAT) << QStringLiteral(
		"Network congestion no longer detected, exiting saturation mode");

	// Exit saturation mode and reset timer just to be safe
	m_gamerInSatMode = false;
	m_gamerSatModeTimer = 0.0f;

	// Disable Nagle's algorithm
	m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
}

void RTMPClient::socketConnected()
{
	Q_ASSERT(m_handshakeState == ConnectingState);
	m_handshakeState = ConnectedState;
	emit connected();
	if(m_handshakeState != ConnectedState)
		return; // Above slot disconnected our connection

	// Create socket notifier and disable it immediately due to Windows
	// implementation issues (See QSocketNotifier documentation). Note that we
	// will received a "Multiple socket notifiers for same socket" warning from
	// Qt as QTcpSocket already has an internal notifier that we can't access.
	Q_ASSERT(m_socketWriteNotifier == NULL);
	m_socketWriteNotifier = new QSocketNotifier(
		m_socket.socketDescriptor(), QSocketNotifier::Write, this);
	m_socketWriteNotifier->setEnabled(false);
	QObject::connect(m_socketWriteNotifier, &QSocketNotifier::activated,
		this, &RTMPClient::socketReadyForWrite);

	// We only disable Nagle's algorithm when in gamer mode as we use our own
	// packet reduction algorithm. Nagle's algorithm is enabled again if we
	// ever enter "saturation mode".
	if(s_inGamerMode)
		m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);

	if(m_autoInitialize) {
		if(!initialize()) {
			broLog(LOG_CAT, BroLog::Warning)
				<< QStringLiteral("Failed to initiate RTMP handshake process");
			disconnect();
		}
	}
}

void RTMPClient::socketDisconnected()
{
	if(m_publisher != NULL) {
		delete m_publisher;
		m_publisher = NULL;
	}
	if(m_socketWriteNotifier != NULL) {
		delete m_socketWriteNotifier;
		m_socketWriteNotifier = NULL;
	}

	m_handshakeState = DisconnectedState;
	m_outBuf.clear();
	m_inBuf.clear();
	m_gamerOutBuf.clear();
	emit disconnected();
}

void RTMPClient::socketError(QAbstractSocket::SocketError err)
{
	broLog(LOG_CAT, BroLog::Warning)
		<< QStringLiteral("Received network socket error: %1")
		.arg(getSocketErrorString(err));
	switch(err) {
	case QAbstractSocket::ConnectionRefusedError:
		emit error(ConnectionRefusedError);
		break;
	case QAbstractSocket::RemoteHostClosedError:
		emit error(RemoteHostClosedError);
		break;
	case QAbstractSocket::HostNotFoundError:
		emit error(HostNotFoundError);
		break;
	case QAbstractSocket::SocketTimeoutError:
		emit error(TimeoutError);
		break;
	case QAbstractSocket::NetworkError:
		emit error(NetworkError);
		break;
	case QAbstractSocket::SslHandshakeFailedError:
		emit error(SslHandshakeFailedError);
		break;
	default:
	case QAbstractSocket::UnknownSocketError:
		emit error(UnknownError);
		break;
	}
	if(m_handshakeState == ConnectingState) {
		// Failed to connect, reset state
		m_socket.abort(); // Make sure that the socket is closed
		m_handshakeState = DisconnectedState;
		emit disconnected();
	}
}

/// <summary>
/// Called when we detect that the remote host closed the connection but Qt
/// never notified us that it happened.
/// </summary>
void RTMPClient::socketRemoteDisconnectTimeout()
{
	if(isSocketConnected() &&
		(m_socket.state() == QAbstractSocket::UnconnectedState ||
		m_socket.socketDescriptor() == -1))
	{
		socketError(QAbstractSocket::RemoteHostClosedError);
	}
}

/// <summary>
/// Called whenever the OS's TCP socket buffer can accept more data.
/// </summary>
void RTMPClient::socketReadyForWrite()
{
	//broLog() << "Socket ready for write";
	// Write any pending data to the socket if we're not in forced buffer mode
	if(m_bufferOutBufRef > 0)
		return;
	attemptToEmptyOutBuf(true);
}

/// <summary>
/// Called whenever new data is ready to be read from the network socket.
/// </summary>
void RTMPClient::socketDataReady()
{
	// We cannot rely on Qt to emit "readyRead()" signals if there are any
	// bytes left in Qt's read buffer as it results in deadlocks. Copy
	// everything into our own buffer and then process from that. We also need
	// to keep in mind that we are operating the Qt socket in "unbuffered"
	// mode so we need a buffer anyway in order to receive large messages.
	for(;;) {
		// We cannot rely on "bytesAvailable()" to properly return the actual
		// amount of bytes available from an "unbuffered" TCP socket.
		qint64 oldSize = m_inBuf.size();
		m_inBuf += m_socket.readAll();
		if(oldSize == m_inBuf.size())
			break;
		//broLog() << "In buffer size: " << m_inBuf.size();

		QBuffer buffer(&m_inBuf);
		buffer.open(QBuffer::ReadOnly);
		processSocketData(buffer);

		// Remove read data from the buffer
		//broLog() << "Read " << buffer.pos() << " bytes";
		if(buffer.pos() >= m_inBuf.size())
			m_inBuf.clear();
		else if(buffer.pos() > 0)
			m_inBuf = m_inBuf.right(m_inBuf.size() - buffer.pos());
	}
	//broLog() << "In buffer size at output: " << m_inBuf.size();
}

/// <summary>
/// Attempt to process any received socket data.
/// </summary>
void RTMPClient::processSocketData(QBuffer &buffer)
{
	// Create stream for easy reading of data
	QDataStream stream(&buffer);
	stream.setByteOrder(QDataStream::BigEndian);
	stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
	stream.setVersion(12);

	// Handle handshake responses
	switch(m_handshakeState) {
	default:
	case DisconnectedState:
	case ConnectingState:
	case DisconnectingState:
		// No data should ever be sent in these states, truncate input
		buffer.readAll();
		return;
	case ConnectedState:
		// The server should never send any data in this state, truncate input
		buffer.readAll();
		return;
	case VersionSentState: {
		// We are waiting for a server S0
		if(buffer.bytesAvailable() < 1)
			return;
#if DEBUG_RTMP_HANDSHAKE
		broLog(LOG_CAT) << "Received possible S0";
#endif
		quint8 version;
		stream >> version;
		if(version != 3) { // "3" = RTMP v1.0
			broLog(LOG_CAT, BroLog::Warning)
				<< QStringLiteral("Received invalid server RTMP version: %1")
				.arg(version);
			emit error(UnexpectedResponseError);
			disconnect();
			return;
		}
#if DEBUG_RTMP_HANDSHAKE
		broLog(LOG_CAT) << "Received valid S0";
#endif
		m_handshakeState = VersionReceivedState;
		/* Fall through */ }
	case VersionReceivedState: {
		// We are waiting for a server S1
		if(buffer.bytesAvailable() < 4 + 4 + 1528)
			return;
#if DEBUG_RTMP_HANDSHAKE
		broLog(LOG_CAT) << "Received S1";
#endif
		quint32 time, zero;
		stream >> time;
		stream >> zero;
		QByteArray echo = buffer.read(1528);
		if(!writeC2S2(time, echo)) {
			disconnect();
			return;
		}
		m_handshakeState = AckSentState;
		/* Fall through */ }
	case AckSentState: {
		// We are waiting for a server S2
		if(buffer.bytesAvailable() < 4 + 4 + 1528)
			return;
#if DEBUG_RTMP_HANDSHAKE
		broLog(LOG_CAT) << "Received possible S2";
#endif
		quint32 time, time2;
		stream >> time;
		stream >> time2;
		QByteArray echo = buffer.read(1528);

		// Validate echo. We ignore the times as some servers (Such as
		// nginx-rtmp) do not follow the specification and send invalid times.
		if(m_handshakeRandomData != echo) {
			broLog(LOG_CAT, BroLog::Warning)
				<< QStringLiteral("Received incorrect handshake echo from server");
			emit error(UnexpectedResponseError);
			disconnect();
			return;
		}
#if DEBUG_RTMP_HANDSHAKE
		broLog(LOG_CAT) << "Received valid S2";
#endif

		// Reset RTMP connection state
		resetStateMembers();

		m_handshakeState = InitializedState;
		emit initialized();

		// Begin RTMP "connect()"
		if(m_autoAppConnect) {
			if(!connectToApp()) {
				broLog(LOG_CAT, BroLog::Warning)
					<< QStringLiteral("Failed to initiate RTMP application connection process");
				disconnect();
				return;
			}
		}

		/* Fall through */ }
	case InitializedState:
		// All other RTMP traffic. Read all available chunks.
		while(readChunkFromSocket(buffer));
		break;
	}
}

/// <summary>
/// Reads one chunk from the socket if one exists.
/// </summary>
/// <returns>True if a chunk was read</returns>
bool RTMPClient::readChunkFromSocket(QBuffer &buffer)
{
	// Due to RTMP's variable length headers we cannot use a QDataStream so the
	// code below is slightly ugly.

	// Max header size is the 3 byte "basic header" + 11 byte "type 0 message
	// header" + 4 byte extended timestamp.
	const int MAX_CHUNK_HEADER_SIZE = 3 + 11 + 4;
	QByteArray header = buffer.peek(MAX_CHUNK_HEADER_SIZE);
	const char *data = header.constData();
	int basicSize = 1;

	// Decode the "basic header"
	if(header.size() < basicSize)
		return false; // No more data to read
	uint fmt = (data[0] & 0xC0) >> 6;
	uint csId = (data[0] & 0x3F);
	if(csId == 0) {
		basicSize = 2;
		if(header.size() < basicSize)
			return false; // Not enough data in buffer
		csId = (uint)data[1] + 64;
	} else if(csId == 1) {
		basicSize = 3;
		if(header.size() < basicSize)
			return false; // Not enough data in buffer
		csId = (uint)data[2] * 256 + (uint)data[1] + 64;
	}

	// Initialize input chunk stream state if it's a new chunk stream
	bool isNew = initInChunkStreamState(csId);
	if(isNew) {
#if DEBUG_LOW_LEVEL_RTMP
		broLog(LOG_CAT)
			<< QStringLiteral("New input chunk stream ID: %L1").arg(csId);
#endif // DEBUG_LOW_LEVEL_RTMP
	}

	// Decode the "message header"
	int dataStart = basicSize;
	int chunkLen = 0;
	bool doAbort = false;
	ChunkStreamState state = m_inChunkStreams[csId];
	switch(fmt) {
	default: // It's impossible to get a result that's outside 0-3
	case 0:
		dataStart = basicSize + 11;
		if(header.size() < dataStart)
			return false; // Not enough data in buffer
		state.timestamp = amfDecodeUInt24(&data[basicSize]);
		if(state.timestamp >= 0xFFFFFF) {
			// Timestamp is in the extended header
			dataStart += 4;
			if(header.size() < dataStart)
				return false; // Not enough data in buffer
			state.timestamp = amfDecodeUInt32(&data[basicSize+11]);
		}
		state.timestampDelta = state.timestamp; // Specification is weird
		state.msgLen = amfDecodeUInt24(&data[basicSize+3]);
		if(state.msgLenRemaining > 0)
			doAbort = true;
		state.msgLenRemaining = state.msgLen;
		chunkLen = qMin(state.msgLen, m_inMaxChunkSize);
		state.msgType = (RTMPMsgType)data[basicSize+6];
		// TODO: Validate message type
		// The message stream ID is the only little-endian field in RTMP
		state.msgStreamId = decodeLEUInt32(&data[basicSize+7]);
		break;
	case 1:
		dataStart = basicSize + 7;
		if(header.size() < dataStart)
			return false; // Not enough data in buffer
		state.timestampDelta = amfDecodeUInt24(&data[basicSize]);
		state.timestamp += state.timestampDelta;
		state.msgLen = amfDecodeUInt24(&data[basicSize+3]);
		if(state.msgLenRemaining > 0)
			doAbort = true;
		state.msgLenRemaining = state.msgLen;
		chunkLen = qMin(state.msgLen, m_inMaxChunkSize);
		state.msgType = (RTMPMsgType)data[basicSize+6];
		// TODO: Validate message type
		break;
	case 2:
		dataStart = basicSize + 3;
		if(header.size() < dataStart)
			return false; // Not enough data in buffer
		state.timestampDelta = amfDecodeUInt24(&data[basicSize]);
		state.timestamp += state.timestampDelta;
		// Due to ambiguities in the specification we are lenient here to allow
		// the remote host to send "type 2" headers for setting the delta to 0
		// when splitting a message into chunks.
		if(state.msgLenRemaining > 0) {
			// Continuation of the previous chunk
			chunkLen = qMin(state.msgLenRemaining, m_inMaxChunkSize);
		} else {
			// Brand new message
			state.msgLenRemaining = state.msgLen;
			chunkLen = qMin(state.msgLen, m_inMaxChunkSize);
		}
		break;
	case 3:
		dataStart = basicSize;
		// WARNING: The RTMP specification contradicts itself about how
		// timestamp deltas are handled for this header type. The first
		// specification example (Section 5.3.2.1) shows that the delta is
		// added to the previous timestamp while the second example (Section
		// 5.3.2.2) shows that it is not. We therefore assume that the delta is
		// only applied if the previous message was not split across multiple
		// chunks.
		if(state.msgLenRemaining > 0) {
			// Continuation of the previous chunk
			chunkLen = qMin(state.msgLenRemaining, m_inMaxChunkSize);
		} else {
			// Brand new message
			state.timestamp += state.timestampDelta;
			state.msgLenRemaining = state.msgLen;
			chunkLen = qMin(state.msgLen, m_inMaxChunkSize);
		}
		break;
	}
	if(buffer.bytesAvailable() < dataStart + chunkLen)
		return false; // Not enough data in buffer
	if(fmt != 3) {
		// Allocate memory for new message
		state.msg.clear();
		state.msg.reserve(state.msgLen);
	}
	// If we have gotten here then the entire chunk has been received

#if DEBUG_LOW_LEVEL_RTMP
	broLog(LOG_CAT)
		<< QStringLiteral("  << Received chunk type %1 of size %L2 from chunk stream %L3")
		.arg(fmt).arg(chunkLen).arg(csId);
#endif // DEBUG_LOW_LEVEL_RTMP

	// Log when a message was discarded by an unexpected chunk type. We don't
	// need to disconnect as our connection state is still valid (Although
	// higher level state machines might now be invalid).
	if(doAbort) {
		broLog(LOG_CAT, BroLog::Warning)
			<< QStringLiteral("Message aborted without an abort message");
		//emit error(UnexpectedResponseError);
	}

	// Copy chunk payload to our message buffer
	buffer.read(dataStart); // Discard header
	state.msg += buffer.read(chunkLen);
	state.msgLenRemaining -= chunkLen;
	m_inChunkStreams[csId] = state; // Apply updated state

	// Send acknowledge ASAP after receiving the specified amount of data
	m_inBytesSinceHandshake += dataStart + chunkLen;
	m_inBytesSinceLastAck += dataStart + chunkLen;
	if(m_inBytesSinceLastAck >= m_inAckWinSize) {
		// Acknowledge everything. If we want to be able to throttle the remote
		// host then we will need to do something more complex than this.
		writeAcknowledge();

		// Not sure if our counter should be reset to 0 or just decreased by
		// the window size. We assume that it's reset so we don't have issues
		// if the window size is reduced during the session.
		m_inBytesSinceLastAck = 0;
	}

	// Process message if it is complete
	if(state.msgLenRemaining <= 0) {
		processMessage(
			state.msgStreamId, state.msgType, state.timestamp, state.msg);
	}

	return true;
}

/// <summary>
/// Process the received RTMP message.
/// </summary>
void RTMPClient::processMessage(
	uint streamId, RTMPMsgType type, quint32 timestamp, const QByteArray &msg)
{
	switch(type) {
	case SetChunkSizeMsgType:
		if(msg.size() < 4) {
			// TODO: Log reason
			emit error(UnexpectedResponseError);
			disconnect();
			return;
		}
		m_inMaxChunkSize = amfDecodeUInt32(msg.constData()) & 0x7FFFFFFF;
		break;
	case AckMsgType:
		if(msg.size() < 4) {
			// TODO: Log reason
			emit error(UnexpectedResponseError);
			disconnect();
			return;
		}
		// Ignore acknowledgements for now, TODO
		break;
	case UserControlMsgType: {
		if(msg.size() < 2) {
			// TODO: Log reason
			emit error(UnexpectedResponseError);
			disconnect();
			return;
		}
		UserControlType type =
			(UserControlType)amfDecodeUInt16(msg.constData());
		switch(type) {
		case StreamBeginType:
			// TODO
			break;
		case StreamEofType:
			// TODO
			break;
		case StreamDryType:
			// TODO
			break;
		case SetBufLenType:
			// Clients should not receive this
			break;
		case StreamIfRecordedType:
			// TODO
			break;
		case PingRequestType:
			// Immediately respond with a ping reply
			writePingResponse(amfDecodeUInt32(&msg.constData()[2]));
			break;
		case PingResponseType:
			// Clients should not receive this
			break;
		default:
			// Unknown user control message, ignore it
			broLog(LOG_CAT, BroLog::Warning)
				<< QStringLiteral("Unknown user control message received (%L1), ignoring")
				.arg((uint)type);
			break;
		}
		break; }
	case WindowAckSizeMsgType:
		if(msg.size() < 4) {
			// TODO: Log reason
			emit error(UnexpectedResponseError);
			disconnect();
			return;
		}
		m_inAckWinSize = amfDecodeUInt32(msg.constData());
		break;
	case SetPeerBWMsgType: {
		if(msg.size() < 5) {
			// TODO: Log reason
			emit error(UnexpectedResponseError);
			disconnect();
			return;
		}
		uint winSize = amfDecodeUInt32(msg.constData());
		AckLimitType type = (AckLimitType)amfDecodeUInt8(&msg.constData()[4]);
		switch(type) {
		case HardLimitType:
			// From RTMP specification:
			// "The peer SHOULD limit its output bandwidth to the indicated
			// window size."
			setAckWinSize(winSize);
			break;
		case SoftLimitType:
			// From RTMP specification:
			// "The peer SHOULD limit its output bandwidth to the the window
			// indicated in this message or the limit already in effect,
			// whichever is smaller."
			if(winSize > m_outAckWinSize)
				setAckWinSize(winSize);
			break;
		case DynamicLimitType:
			// From RTMP specification:
			// "If the previous Limit Type was Hard, treat this message as
			// though it was marked Hard, otherwise ignore this message."
			if(m_inAckLimitType == HardLimitType)
				setAckWinSize(winSize);
			break;
		default:
			// Unknown type, ignore
			break;
		}
		break; }
	case CommandAmf0MsgType: {
		if(msg.size() < 1) {
			// TODO: Log reason
			emit error(UnexpectedResponseError);
			disconnect();
			return;
		}

		// Decode AMF message
		AMFTypeList params;
		int off = 0;
		while(off < msg.count()) {
			AMFType *amfObj;
			uint bytesRead = AMFType::decode(&msg.constData()[off], &amfObj);
			if(bytesRead == 0) {
				broLog(LOG_CAT, BroLog::Warning)
					<< QStringLiteral("Failed to decode AMF message");
				emit error(UnexpectedResponseError);
				disconnect();
				return;
			}
			if(amfObj == NULL) {
				broLog(LOG_CAT, BroLog::Warning)
					<< QStringLiteral("Failed to decode AMF message but still read %L1 bytes")
					.arg(bytesRead);
				emit error(UnexpectedResponseError);
				disconnect();
				return;
			}
			off += bytesRead;
			if(off > msg.count()) {
				// Buffer overflow
				broLog(LOG_CAT, BroLog::Warning)
					<< QStringLiteral("Buffer overflow while decoding AMF message");
				emit error(UnexpectedResponseError);
				disconnect();
				return;
			}
			params.append(amfObj);
		}
		if(params.count() == 0) {
			// Ignore empty messages
			break;
		}

#if DEBUG_LOW_LEVEL_RTMP
		broLog(LOG_CAT) << "  << Received AMF message: --------";
		for(int i = 0; i < params.count(); i++)
			broLog(LOG_CAT) << params.at(i);
		broLog(LOG_CAT) << "--------";
#endif // DEBUG_LOW_LEVEL_RTMP

		// Emit to listeners that we received a message
		emit receivedAmfCommandMsg(streamId, params);

		// Is it an internal message?
		AMFString *invoke = params.at(0)->asString();
		if((*invoke == QStringLiteral("_result") ||
			*invoke == QStringLiteral("_error")) && params.count() >= 4)
		{
			// Result message
			bool isError = (*invoke == QStringLiteral("_error"));

			AMFNumber *transId = params.at(1)->asNumber();
			if(!m_appConnected &&
				transId->getValue() == m_appConnectTransId)
			{
				// This message is the result of our "connect()"
				if(!isError) {
					// TODO: Parse server information?
					m_appConnected = true;
					emit connectedToApp();
				} else {
					// Rejected from server
					broLog(LOG_CAT, BroLog::Warning)
						<< QStringLiteral("RTMP application connection rejected");
					//. Reason = %1").arg(); // TODO
					emit error(RtmpConnectRejectedError);
					disconnect();
					return;
				}
			} else if(m_creatingStream &&
				transId->getValue() == m_createStreamTransId)
			{
				// This message is the result of our "createStream()"
				m_creatingStream = false;
				m_createStreamTransId = 0;
				if(!isError) {
					AMFNumber *streamId = params.at(3)->asNumber();
					if(streamId != NULL) { // TODO: Handle failure
						emit createdStream(streamId->getValue());

						// HACK/TODO: We assume only one stream is created per
						// connection
						if(m_publisher != NULL) {
							m_publishStreamId = streamId->getValue();

							// Begin publishing immediately
							writePublishMsg(m_publishStreamId);
						}
					}
				} else {
					// Error creating stream, TODO: We probably don't need to
					// disconnect but the application most likely doesn't
					// handle the failure case anyway.
					broLog(LOG_CAT, BroLog::Warning)
						<< QStringLiteral("RTMP stream creation failed");
					//. Reason = %1").arg(); // TODO
					emit error(RtmpCreateStreamError);
					disconnect();
					return;
				}
			}
		} else if(m_beginningPublish &&
			*invoke == QStringLiteral("onStatus") && params.count() >= 4 &&
			streamId == m_publishStreamId)
		{
			// Our "publish()" has completed
			m_beginningPublish = false;
			m_lastPublishTimestamp = 0;

			AMFObject *result = params.at(3)->asObject();
			if(result == NULL || !result->contains("code")) {
				emit error(UnexpectedResponseError);
				disconnect();
				return;
			}
			AMFString *code = result->value("code")->asString();
			if(code == NULL) {
				emit error(UnexpectedResponseError);
				disconnect();
				return;
			}
			if(*code == QStringLiteral("NetStream.Publish.Start")) {
				// Server accepted publish
				m_publisher->setReady(true);
			} else {
				// Server rejected publish
				broLog(LOG_CAT, BroLog::Warning)
					<< QStringLiteral("Server rejected publish. Reason = %1")
					.arg(*code);
				emit error(RtmpPublishRejectedError);
				disconnect();
			}
		}

		// Release memory
		for(int i = 0; i < params.count(); i++)
			delete params.at(i);
		params.clear();

		break; }
	default:
#if DEBUG_LOW_LEVEL_RTMP
		broLog(LOG_CAT, BroLog::Warning)
			<< QStringLiteral("  << Received unknown message type %L1 of size %L2 from stream %L3")
			.arg((uint)type).arg(msg.size()).arg(streamId);
#else
		broLog(LOG_CAT, BroLog::Warning)
			<< QStringLiteral("Received unknown message type %L1 of size %L2 from stream %L3")
			.arg((uint)type).arg(msg.size()).arg(streamId);
#endif // DEBUG_LOW_LEVEL_RTMP
	}
}

/// <returns>True if it is a new chunk stream</returns>
bool RTMPClient::initInChunkStreamState(int id)
{
	if(m_inChunkStreams.contains(id))
		return false; // Already exists
	ChunkStreamState state;
	state.timestamp = 0;
	state.timestampDelta = 0;
	state.msgLen = 0;
	state.msgType = NullMsgType;
	state.msgStreamId = 0;
	state.msgLenRemaining = 0;
	state.msg = QByteArray();
	m_inChunkStreams[id] = state;
	return true;
}

/// <returns>True if it is a new chunk stream</returns>
bool RTMPClient::initOutChunkStreamState(int id)
{
	if(m_outChunkStreams.contains(id))
		return false; // Already exists
	ChunkStreamState state;
	state.timestamp = 0;
	state.timestampDelta = 0;
	state.msgLen = 0;
	state.msgType = NullMsgType;
	state.msgStreamId = 0;
	state.msgLenRemaining = 0;
	state.msg = QByteArray();
	m_outChunkStreams[id] = state;
	return true;
}
