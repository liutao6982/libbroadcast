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

#ifndef RTMPCLIENT_H
#define RTMPCLIENT_H

#include "rtmptargetinfo.h"
#include <QtCore/QBuffer>
#include <QtCore/QDataStream>
#include <QtCore/QObject>
#include <QtCore/QSocketNotifier>
#include <QtNetwork/QTcpSocket>

class AMFObject;
class AMFType;
class RTMPClient;

typedef QVector<AMFType *> AMFTypeList;

//=============================================================================
/// <summary>
/// Represents a "publish()" RTMP stream. WARNING: Created objects are
/// invalidated and deleted when the RTMP connection closes due to any reason.
/// The application is expected to listen for error or disconnect signals to
/// prevent crashes!
/// </summary>
class LBC_EXPORT RTMPPublisher : public QObject
{
	friend class RTMPClient;
	Q_OBJECT

private: // Members -----------------------------------------------------------
	RTMPClient *	m_client;
	bool			m_isReady;
	bool			m_isAvc;

private: // Constructor/destructor ---------------------------------------------
	RTMPPublisher(RTMPClient *client);
	virtual ~RTMPPublisher();

public: // Methods ------------------------------------------------------------
	bool			isReady() const;

	bool			beginPublishing();
	bool			finishPublishing();

	void			beginForceBufferWrite();
	void			endForceBufferWrite();
	bool			willWriteBuffer() const;
	bool			writeDataFrame(AMFObject *data);
	bool			writeAvcConfigRecord(
		const QByteArray &sps, const QByteArray &pps);
	bool			writeAacSequenceHeader(const QByteArray &oob);
	bool			writeVideoFrame(
		quint32 timestamp, const QByteArray &header, QVector<QByteArray> pkts);
	bool			writeAudioFrame(
		quint32 timestamp, const QByteArray &header, const QByteArray &data);

private:
	void			setReady(bool isReady);

Q_SIGNALS: // Signals ---------------------------------------------------------
	void			ready();

	/// <summary>
	/// Emitted whenever the network socket can accept more data. For maximum
	/// efficiency the application should write the minimum amount of data that
	/// it can without splitting frames or segments that is greater or equal to
	/// `numBytes` in size.
	/// </summary>
	void			socketDataRequest(uint numBytes);
};
//=============================================================================

inline bool RTMPPublisher::isReady() const
{
	return m_isReady;
}

//=============================================================================
class LBC_EXPORT RTMPClient : public QObject
{
	friend class RTMPPublisher;
	friend class RTMPClientInitializeTest;
	Q_OBJECT

public: // Datatypes ----------------------------------------------------------
	enum HandshakeState {
		DisconnectedState = 0,
		ConnectingState,
		ConnectedState,
		VersionSentState,
		VersionReceivedState,
		AckSentState,
		InitializedState,
		DisconnectingState
	};
	enum RTMPError { // Declared as a Qt metatype below the class
		UnknownError = 0,
		ConnectionRefusedError,
		RemoteHostClosedError,
		HostNotFoundError,
		TimeoutError,
		NetworkError,
		SslHandshakeFailedError,
		UnexpectedResponseError,
		InvalidWriteError,
		RtmpConnectRejectedError,
		RtmpCreateStreamError,
		RtmpPublishRejectedError
	};
	enum AckLimitType {
		HardLimitType = 0,
		SoftLimitType = 1,
		DynamicLimitType = 2
	};

private:
	enum RTMPMsgType {
		NullMsgType = 0, // Unofficial
		SetChunkSizeMsgType = 1,
		AbortMsgType = 2,
		AckMsgType = 3,
		UserControlMsgType = 4,
		WindowAckSizeMsgType = 5,
		SetPeerBWMsgType = 6,
		// ...
		AudioMsgType = 8,
		VideoMsgType = 9,
		// ...
		DataAmf3MsgType = 15,
		SharedObjAmf3MsgType = 16,
		CommandAmf3MsgType = 17,
		DataAmf0MsgType = 18,
		SharedObjAmf0MsgType = 19,
		CommandAmf0MsgType = 20,
		// ...
		AggregateMsgType = 22
	};
	enum UserControlType {
		StreamBeginType = 0,
		StreamEofType = 1,
		StreamDryType = 2,
		SetBufLenType = 3,
		StreamIfRecordedType = 4,
		// ...
		PingRequestType = 6,
		PingResponseType = 7
	};

	struct ChunkStreamState {
		// Explicitly in specification
		quint32		timestamp;
		uint		timestampDelta;
		uint		msgLen;
		RTMPMsgType	msgType;
		uint		msgStreamId;

		// Implicit states
		uint		msgLenRemaining;
		QByteArray	msg;
	};

private: // Static members ----------------------------------------------------
	static bool		s_inGamerMode;
	static float	s_gamerTickFreq;

private: // Members -----------------------------------------------------------
	RTMPTargetInfo		m_remoteInfo;
	QTcpSocket			m_socket;
	QSocketNotifier *	m_socketWriteNotifier;
	bool				m_autoInitialize;
	bool				m_autoAppConnect;
	QString				m_versionString;
	RTMPPublisher *		m_publisher;

	// Connection state
	HandshakeState	m_handshakeState;
	QByteArray		m_handshakeRandomData;
	uint			m_inMaxChunkSize;
	uint			m_outMaxChunkSize;
	uint			m_inAckWinSize;
	uint			m_outAckWinSize;
	AckLimitType	m_inAckLimitType;
	uint			m_inBytesSinceLastAck;
	uint			m_outBytesSinceLastAck;
	uint			m_inBytesSinceHandshake;
	QHash<uint, ChunkStreamState>	m_inChunkStreams;
	QHash<uint, ChunkStreamState>	m_outChunkStreams;
	QHash<uint, uint>				m_nextTransactionIds;
	bool			m_appConnected; // RTMP "connect()" completed
	uint			m_appConnectTransId;
	bool			m_creatingStream; // "createStream()"
	uint			m_createStreamTransId;
	uint			m_publishStreamId;
	bool			m_beginningPublish; // "publish()"
	quint32			m_lastPublishTimestamp;

	// Input/output buffers
	QByteArray		m_outBuf; // Output TCP socket buffer
	int				m_bufferOutBufRef; // Force buffer writes
	QBuffer			m_writeStreamBuf;
	QDataStream		m_writeStream;
	QByteArray		m_inBuf; // Input TCP socket buffer

	// Gamer mode
	int				m_gamerBytesLeft;
	QByteArray		m_gamerOutBuf; // Internal output buffer
	int				m_gamerAvgUploadBytes; // Approx. bytes per second
	bool			m_gamerInSatMode; // In saturation mode
	float			m_gamerSatModeTimer; // Timer for exiting saturation mode
	float			m_gamerExitSatModeTime; // Time to exit saturation mode

public: // Static methods -----------------------------------------------------
	static QString	errorToString(RTMPClient::RTMPError error);

	static void		gamerModeSetEnabled(bool enabled);
	static void		gamerSetTickFreq(float freq);

public: // Constructor/destructor ---------------------------------------------
	RTMPClient();
	virtual ~RTMPClient();

public: // Methods ------------------------------------------------------------
	RTMPTargetInfo	getRemoteTarget() const;
	HandshakeState	getHandshakeState() const;
	bool			isSocketConnected() const;
	//int				getBufferSize() const;
	void			setAutoInitialize(bool autoInitialize);
	bool			getAutoInitialize() const;
	void			setAutoConnectToApp(bool autoAppConnect);
	bool			getAutoConnectToApp() const;
	void			setVersionString(const QString &string);
	QString			getVersionString() const;

	bool			setRemoteTarget(const RTMPTargetInfo &info);
	bool			setRemoteTarget(const QString &url);

	// Connection control
	bool			connect();
	bool			initialize();
	bool			connectToApp();
	RTMPPublisher *	createPublishStream();
	void			deletePublishStream();
	void			disconnect(bool cleanDisconnect = true);
	int				getOSWriteBufferSize() const;
	int				setOSWriteBufferSize(int size);

	// Abstracted RTMP commands
	bool			setMaxChunkSize(uint maxSize);
	bool			setAckWinSize(uint ackWinSize);
	bool			setPeerBandwidth(uint ackWinSize, AckLimitType limitType);

	// Gamer mode
	void			gamerSetAverageUpload(int avgUploadBytes);
	void			gamerSetExitSatModeTime(float exitTime);
	void			gamerTickEvent(int numDropped);
private:
	void			gamerEnterSatMode();
	void			gamerExitSatMode();

private:
	// Generic writing methods
	bool			write(const QByteArray &data);
	int				socketWrite(
		const QByteArray &data, bool emitDataRequest = false);
	void			beginForceBufferWrite();
	void			endForceBufferWrite();
	bool			attemptToEmptyOutBuf(bool emitDataRequest = false);
	bool			willWriteBuffer() const;
	QDataStream *	beginWriteStream();
	bool			endWriteStream(bool writeToBuffer = true);
	uint			getNextTransactionId(uint streamId);

	// Specific writing methods
	bool			writeC0S0();
	bool			writeC1S1();
	bool			writeC2S2(quint32 time, const QByteArray &echo);
	bool			writeMessage(
		uint streamId, RTMPMsgType type, quint32 timestamp,
		const QByteArray &msg, uint chunkStreamId);
	bool			writeAcknowledge();
	bool			writePingResponse(uint timestamp);
	bool			writeVideoData(uint timestamp, const QByteArray &data);
	bool			writeAudioData(uint timestamp, const QByteArray &data);

	// Specific writing methods for AMF 0 commands
	bool			writeConnectMsg(uint transactionId);
	bool			writeCreateStreamMsg();
	bool			writeDeleteStreamMsg(uint streamId);
	bool			writePublishMsg(uint streamId);
	bool			writeSetDataFrameMsg(AMFObject *streamData);

	// Miscellaneous
	void			resetStateMembers();
	void			processSocketData(QBuffer &buffer);
	bool			readChunkFromSocket(QBuffer &buffer);
	void			processMessage(
		uint streamId, RTMPMsgType type, quint32 timestamp,
		const QByteArray &msg);
	bool			initInChunkStreamState(int id);
	bool			initOutChunkStreamState(int id);

Q_SIGNALS: // Signals ---------------------------------------------------------
	void			connecting();
	void			connected();
	void			initialized(); // Handshake complete
	void			connectedToApp(); // "connect()" command complete
	void			createdStream(uint streamId);
	void			disconnected();
	void			error(RTMPClient::RTMPError error);
	void			dataWritten(const QByteArray &data);
	void			receivedAmfCommandMsg(
		uint streamId, const AMFTypeList &params);

	private
Q_SLOTS: // Slots -------------------------------------------------------------
	void			socketConnected();
	void			socketDisconnected();
	void			socketError(QAbstractSocket::SocketError err);
	void			socketDataReady();
	void			socketReadyForWrite();
	void			socketRemoteDisconnectTimeout();
};
//=============================================================================

// Declare our signal datatypes with Qt
Q_DECLARE_METATYPE(RTMPClient::RTMPError);

inline RTMPTargetInfo RTMPClient::getRemoteTarget() const
{
	return m_remoteInfo;
}

inline RTMPClient::HandshakeState RTMPClient::getHandshakeState() const
{
	return m_handshakeState;
}

#if 0
/// <summary>
/// Returns the size of the internal output buffer that has not been sent to
/// the operating system yet. This value does not take into account the
/// operating system's network buffer size.
/// </summary>
inline int RTMPClient::getBufferSize() const
{
	// Unimplemented
}
#endif // 0

inline void RTMPClient::setAutoInitialize(bool autoInitialize)
{
	m_autoInitialize = autoInitialize;
}

inline bool RTMPClient::getAutoInitialize() const
{
	return m_autoInitialize;
}

inline void RTMPClient::setAutoConnectToApp(bool autoAppConnect)
{
	m_autoAppConnect = autoAppConnect;
}

inline bool RTMPClient::getAutoConnectToApp() const
{
	return m_autoAppConnect;
}

inline void RTMPClient::setVersionString(const QString &string)
{
	m_versionString = string;
}

inline QString RTMPClient::getVersionString() const
{
	return m_versionString;
}

inline uint RTMPClient::getNextTransactionId(uint streamId)
{
	if(m_nextTransactionIds.contains(streamId))
		return m_nextTransactionIds[streamId]++;
	m_nextTransactionIds[streamId] = 2;
	return 1; // Transaction ID 0 is reserved
}

#endif // RTMPCLIENT_H
