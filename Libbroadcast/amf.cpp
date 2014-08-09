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

#include "include/amf.h"

//=============================================================================
// Helpers

/// <summary>
/// Decodes a big-endian 8-bit unsigned integer.
/// </summary>
uint amfDecodeUInt8(const char *data)
{
	unsigned char *uc = (unsigned char *)data;
	return (uint)uc[0];
}

/// <summary>
/// Decodes a big-endian 16-bit unsigned integer.
/// </summary>
uint amfDecodeUInt16(const char *data)
{
	unsigned char *uc = (unsigned char *)data;
	return ((uint)uc[0] << 8) |
		(uint)uc[1];
}

/// <summary>
/// Decodes a big-endian 24-bit unsigned integer.
/// </summary>
uint amfDecodeUInt24(const char *data)
{
	unsigned char *uc = (unsigned char *)data;
	return ((uint)uc[0] << 16) |
		((uint)uc[1] << 8) |
		(uint)uc[2];
}

/// <summary>
/// Decodes a big-endian 32-bit unsigned integer.
/// </summary>
uint amfDecodeUInt32(const char *data)
{
	unsigned char *uc = (unsigned char *)data;
	return ((uint)uc[0] << 24) |
		((uint)uc[1] << 16) |
		((uint)uc[2] << 8) |
		(uint)uc[3];
}

/// <summary>
/// Decode a big-endian 64-bit double precision floating point.
/// </summary>
double amfDecodeDouble(const char *data)
{
	// WARNING: Do we need to worry about platforms that store floats in a
	// different byte order than integers?
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
#error Unsupported byte order
#endif
	unsigned char ucVal[8];
	unsigned char *uc = (unsigned char *)data;
	ucVal[0] = uc[7];
	ucVal[1] = uc[6];
	ucVal[2] = uc[5];
	ucVal[3] = uc[4];
	ucVal[4] = uc[3];
	ucVal[5] = uc[2];
	ucVal[6] = uc[1];
	ucVal[7] = uc[0];
	return *((double *)ucVal);
}

/// <summary>
/// Encode a big-endian 8-bit unsigned integer.
/// </summary>
/// <returns>A pointer to the next byte to write</returns>
char *amfEncodeUInt8(char *data, uint val)
{
	unsigned char *uc = (unsigned char *)data;
	uc[0] = val & 0xff;
	return data + 1;
}

/// <summary>
/// Encode a big-endian 16-bit unsigned integer.
/// </summary>
/// <returns>A pointer to the next byte to write</returns>
char *amfEncodeUInt16(char *data, uint val)
{
	unsigned char *uc = (unsigned char *)data;
	uc[0] = (val >> 8) & 0xff;
	uc[1] = val & 0xff;
	return data + 2;
}

/// <summary>
/// Encode a big-endian 24-bit unsigned integer.
/// </summary>
/// <returns>A pointer to the next byte to write</returns>
char *amfEncodeUInt24(char *data, uint val)
{
	unsigned char *uc = (unsigned char *)data;
	uc[0] = (val >> 16) & 0xff;
	uc[1] = (val >> 8) & 0xff;
	uc[2] = val & 0xff;
	return data + 3;
}

/// <summary>
/// Encode a big-endian 32-bit unsigned integer.
/// </summary>
/// <returns>A pointer to the next byte to write</returns>
char *amfEncodeUInt32(char *data, uint val)
{
	unsigned char *uc = (unsigned char *)data;
	uc[0] = (val >> 24) & 0xff;
	uc[1] = (val >> 16) & 0xff;
	uc[2] = (val >> 8) & 0xff;
	uc[3] = val & 0xff;
	return data + 4;
}

/// <summary>
/// Encode a big-endian 64-bit double precision floating point.
/// </summary>
/// <returns>A pointer to the next byte to write</returns>
char *amfEncodeDouble(char *data, double val)
{
	// WARNING: Do we need to worry about platforms that store floats in a
	// different byte order than integers?
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
#error Unsupported byte order
#endif
	unsigned char *ucVal = (unsigned char *)&val;
	unsigned char *uc = (unsigned char *)data;
	uc[0] = ucVal[7];
	uc[1] = ucVal[6];
	uc[2] = ucVal[5];
	uc[3] = ucVal[4];
	uc[4] = ucVal[3];
	uc[5] = ucVal[2];
	uc[6] = ucVal[1];
	uc[7] = ucVal[0];
	return data + 8;
}

/// <summary>
/// Encode an UTF-8 encoded string with a prefixed 16- or 32-bit length.
/// Assumes that `str` was created with `QString::toUtf8()`.
/// </summary>
/// <returns>A pointer to the next byte to write</returns>
char *amfEncodeUtf8String(char *data, const QByteArray &str)
{
	int lenSize = (str.size() > 0xFFFF) ? 4 : 2;
	if(lenSize == 4)
		data = amfEncodeUInt32(data, str.size());
	else
		data = amfEncodeUInt16(data, str.size());
	memcpy(data, str.constData(), str.size());
	return data + str.size();
}

//=============================================================================
// AMFType class

/// <summary>
/// Decodes the provided byte data as an AMF encoded object and outputs it to
/// the pointer specified by `resultOut`. If there was an error during decoding
/// then `resultOut` will be NULL and the method will return 0. It is the
/// caller's responsibility to delete the result once it is finished using it.
/// </summary>
/// <returns>The amount of bytes read from the input.</returns>
uint AMFType::decode(const char *data, AMFType **resultOut)
{
	if(resultOut == NULL)
		return 0; // Invalid input
	*resultOut = NULL;

	// The first byte is always a marker type
	uint marker = amfDecodeUInt8(data);
	switch(marker) {
	default:
		return 0; // Unknown type
	case 0x00: // NumberType
		*resultOut = new AMFNumber(amfDecodeDouble(&data[1]));
		return 1 + 8;
	case 0x01: // BooleanType
		*resultOut = new AMFBoolean(amfDecodeUInt8(&data[1]) != 0);
		return 1 + 1;
	case 0x02: { // StringType
		uint len = amfDecodeUInt16(&data[1]);
		*resultOut = new AMFString(QString::fromUtf8(&data[3], len));
		return 1 + 2 + len; }
	case 0x03: // ObjectType
	case 0x08: { // EcmaArrayType
		uint objSize = 1; // Number of bytes read
		AMFObject *obj;
		if(marker == 0x03)
			obj = new AMFObject();
		else {
			AMFEcmaArray *ecma = new AMFEcmaArray();
			ecma->setAssociativeCount(amfDecodeUInt32(&data[objSize]));
			objSize += 4;
			obj = ecma;
		}
		*resultOut = obj;
		for(;;) {
			// Decode key
			uint len = amfDecodeUInt16(&data[objSize]);
			QString key = QString::fromUtf8(&data[objSize+2], len);
			objSize += 2 + len;

			// Is the value the "object-end-marker"? If it is then we have no
			// more properties to add to the object
			if(amfDecodeUInt8(&data[objSize]) == 0x09) {
				objSize++;
				break;
			}

			// Decode value
			AMFType *value = NULL;
			objSize += decode(&data[objSize], &value);
			if(value == NULL) {
				// Failed to decode value, abort decode
				delete obj;
				*resultOut = NULL;
				return 0;
			}

			// Append key/value pair to the object
			obj->insert(key, value);
		}
		return objSize; }
	case 0x05: // NullType
		*resultOut = new AMFNull();
		return 1;
	case 0x06: // UndefinedType
		*resultOut = new AMFUndefined();
		return 1;
	case 0x0C: { // LongStringType
		uint len = amfDecodeUInt32(&data[1]);
		*resultOut = new AMFString(QString::fromUtf8(&data[5], len));
		return 1 + 4 + len; }
	}

	// Should never reach here
	Q_ASSERT(false);
	return 0;
}

AMFType::AMFType(ValueType type)
	: m_type(type)
	, m_amfVer(0)
{
}

AMFNumber *AMFType::asNumber()
{
	if(m_type != NumberType)
		return NULL;
	return static_cast<AMFNumber *>(this);
}

const AMFNumber *AMFType::asNumber() const
{
	if(m_type != NumberType)
		return NULL;
	return static_cast<const AMFNumber *>(this);
}

AMFBoolean *AMFType::asBoolean()
{
	if(m_type != BooleanType)
		return NULL;
	return static_cast<AMFBoolean *>(this);
}

const AMFBoolean *AMFType::asBoolean() const
{
	if(m_type != BooleanType)
		return NULL;
	return static_cast<const AMFBoolean *>(this);
}

AMFString *AMFType::asString()
{
	if(m_type != StringType && m_type != LongStringType)
		return NULL;
	return static_cast<AMFString *>(this);
}

const AMFString *AMFType::asString() const
{
	if(m_type != StringType && m_type != LongStringType)
		return NULL;
	return static_cast<const AMFString *>(this);
}

AMFObject *AMFType::asObject()
{
	if(m_type != ObjectType)
		return NULL;
	return static_cast<AMFObject *>(this);
}

const AMFObject *AMFType::asObject() const
{
	if(m_type != ObjectType)
		return NULL;
	return static_cast<const AMFObject *>(this);
}

AMFEcmaArray *AMFType::asEcmaArray()
{
	if(m_type != EcmaArrayType)
		return NULL;
	return static_cast<AMFEcmaArray *>(this);
}

const AMFEcmaArray *AMFType::asEcmaArray() const
{
	if(m_type != EcmaArrayType)
		return NULL;
	return static_cast<const AMFEcmaArray *>(this);
}

AMFNull *AMFType::asNull()
{
	if(m_type != NullType)
		return NULL;
	return static_cast<AMFNull *>(this);
}

const AMFNull *AMFType::asNull() const
{
	if(m_type != NullType)
		return NULL;
	return static_cast<const AMFNull *>(this);
}

AMFUndefined *AMFType::asUndefined()
{
	if(m_type != UndefinedType)
		return NULL;
	return static_cast<AMFUndefined *>(this);
}

const AMFUndefined *AMFType::asUndefined() const
{
	if(m_type != UndefinedType)
		return NULL;
	return static_cast<const AMFUndefined *>(this);
}

BroLog operator<<(BroLog log, const AMFType &amf)
{
	return log << amf.debugString();
}

BroLog operator<<(BroLog log, AMFType *amf)
{
	if(amf == NULL)
		return log << QStringLiteral("** NULL pointer **");
	return log << amf->debugString();
}

//=============================================================================
// AMFNumber class

AMFNumber::AMFNumber(double value)
	: AMFType(NumberType)
	, m_value(value)
{
}

AMFNumber::AMFNumber(const AMFNumber &other)
	: AMFType(NumberType)
	, m_value(other.getValue())
{
}

AMFNumber &AMFNumber::operator=(const AMFNumber &other)
{
	m_value = other.getValue();
	return *this;
}

QByteArray AMFNumber::serialized() const
{
	if(m_amfVer == 0) {
		QByteArray data(9, 0);
		char *ptr = data.data();
		ptr = amfEncodeUInt8(ptr, 0x00); // Marker
		ptr = amfEncodeDouble(ptr, m_value);
		return data;
	} else if(m_amfVer == 3) {
		// TODO: AMF 3 support
		return QByteArray();
	}

	// Unknown AMF version
	return QByteArray();
}

QString AMFNumber::debugString(int indent) const
{
	return QString::number(m_value, 'g', 12);
}

//=============================================================================
// AMFBoolean class

AMFBoolean::AMFBoolean(bool value)
	: AMFType(BooleanType)
	, m_value(value)
{
}

AMFBoolean::AMFBoolean(const AMFBoolean &other)
	: AMFType(BooleanType)
	, m_value(other.getValue())
{
}

AMFBoolean &AMFBoolean::operator=(const AMFBoolean &other)
{
	m_value = other.getValue();
	return *this;
}

QByteArray AMFBoolean::serialized() const
{
	if(m_amfVer == 0) {
		QByteArray data(2, 0);
		char *ptr = data.data();
		ptr = amfEncodeUInt8(ptr, 0x01); // Marker
		ptr = amfEncodeUInt8(ptr, m_value ? 1 : 0);
		return data;
	} else if(m_amfVer == 3) {
		// TODO: AMF 3 support
		return QByteArray();
	}

	// Unknown AMF version
	return QByteArray();
}

QString AMFBoolean::debugString(int indent) const
{
	return QString::number(m_value);
}

//=============================================================================
// AMFString class

AMFString::AMFString()
	: QString()
	, AMFType(StringType)
{
}

AMFString::AMFString(const QString &str)
	: QString(str)
	, AMFType(StringType)
{
}

AMFString::AMFString(const AMFString &other)
	: QString(other)
	, AMFType(StringType)
{
}

AMFString &AMFString::operator=(const AMFString &other)
{
	QString::operator=(other);
	return *this;
}

QByteArray AMFString::serialized() const
{
	if(m_amfVer == 0) {
		QByteArray str = toUtf8();
		int lenSize = (str.size() > 0xFFFF) ? 4 : 2;
		QByteArray data(str.size() + lenSize + 1, 0);
		char *ptr = data.data();
		if(lenSize == 2)
			ptr = amfEncodeUInt8(ptr, 0x02); // Marker
		else
			ptr = amfEncodeUInt8(ptr, 0x0C); // Marker
		ptr = amfEncodeUtf8String(ptr, str);
		return data;
	} else if(m_amfVer == 3) {
		// TODO: AMF 3 support
		return QByteArray();
	}

	// Unknown AMF version
	return QByteArray();
}

QString AMFString::debugString(int indent) const
{
	return QStringLiteral("\"%1\"").arg(*this);
}

//=============================================================================
// AMFObject class

AMFObject::AMFObject()
	: QMap<QString, AMFType *>()
	, AMFType(ObjectType)
{
}

AMFObject::AMFObject(const AMFObject &other)
	: QMap<QString, AMFType *>(other)
	, AMFType(ObjectType)
{
}

AMFObject &AMFObject::operator=(const AMFObject &other)
{
	QMap<QString, AMFType *>::operator=(other);
	return *this;
}

AMFObject::~AMFObject()
{
	// We have memory ownership of all our children, clean up
	deepClear();
}

/// <summary>
/// Delete all children and clear the object.
/// </summary>
void AMFObject::deepClear()
{
	QMapIterator<QString, AMFType *> it(*this);
	while(it.hasNext()) {
		it.next();
		delete it.value();
	}
	clear();
}

QByteArray AMFObject::serialized() const
{
	if(m_amfVer == 0) {
		QByteArray data;
		if(m_type == EcmaArrayType) {
			const AMFEcmaArray *ecma = asEcmaArray();
			data = QByteArray(5, 0x08); // Marker
			amfEncodeUInt32(&data.data()[1], ecma->getAssociativeCount());
		} else
			data = QByteArray(1, 0x03); // Marker

		QMapIterator<QString, AMFType *> it(*this);
		while(it.hasNext()) {
			it.next();

			// Append encoded key
			QByteArray str = it.key().toUtf8();
			int lenSize = (str.size() > 0xFFFF) ? 4 : 2;
			QByteArray keyData(str.size() + lenSize, 0);
			amfEncodeUtf8String(keyData.data(), str);
			data += keyData;

			// Append value
			data += it.value()->serialized();
		}

		data += QByteArray(2, 0x00); // "UTF-8-empty"
		data += QByteArray(1, 0x09); // End marker
		return data;
	} else if(m_amfVer == 3) {
		// TODO: AMF 3 support
		return QByteArray();
	}

	// Unknown AMF version
	return QByteArray();
}

QString AMFObject::debugString(int indent) const
{
	QString ret;
	if(m_type == EcmaArrayType) {
		const AMFEcmaArray *ecma = asEcmaArray();
		ret = QStringLiteral("EcmaArray (%L1) {")
			.arg(ecma->getAssociativeCount());
	} else
		ret = QStringLiteral("Object {");
	QMapIterator<QString, AMFType *> it(*this);
	while(it.hasNext()) {
		it.next();
		ret += QStringLiteral("\n%1%2: %3")
			.arg(QString(indent + 4, QChar(' ')))
			.arg(it.key())
			.arg(it.value()->debugString(indent + 4));
	}
	ret += QStringLiteral(" }");
	return ret;
}

//=============================================================================
// AMFEcmaArray class

AMFEcmaArray::AMFEcmaArray()
	: AMFObject()
	, m_associativeCount(0)
{
	m_type = EcmaArrayType;
}

AMFEcmaArray::AMFEcmaArray(const AMFEcmaArray &other)
	: AMFObject(other)
	, m_associativeCount(other.getAssociativeCount())
{
	m_type = EcmaArrayType;
}

AMFEcmaArray &AMFEcmaArray::operator=(const AMFEcmaArray &other)
{
	AMFObject::operator=(other);
	m_type = EcmaArrayType;
	m_associativeCount = other.getAssociativeCount();
	return *this;
}

//=============================================================================
// AMFNull class

AMFNull::AMFNull()
	: AMFType(NullType)
{
}

AMFNull::AMFNull(const AMFNull &other)
	: AMFType(NullType)
{
}

AMFNull &AMFNull::operator=(const AMFNull &other)
{
	return *this;
}

QByteArray AMFNull::serialized() const
{
	if(m_amfVer == 0) {
		QByteArray data(1, 0);
		char *ptr = data.data();
		ptr = amfEncodeUInt8(ptr, 0x05); // Marker
		return data;
	} else if(m_amfVer == 3) {
		// TODO: AMF 3 support
		return QByteArray();
	}

	// Unknown AMF version
	return QByteArray();
}

QString AMFNull::debugString(int indent) const
{
	return QStringLiteral("NULL");
}

//=============================================================================
// AMFUndefined class

AMFUndefined::AMFUndefined()
	: AMFType(UndefinedType)
{
}

AMFUndefined::AMFUndefined(const AMFUndefined &other)
	: AMFType(UndefinedType)
{
}

AMFUndefined &AMFUndefined::operator=(const AMFUndefined &other)
{
	return *this;
}

QByteArray AMFUndefined::serialized() const
{
	if(m_amfVer == 0) {
		QByteArray data(1, 0);
		char *ptr = data.data();
		ptr = amfEncodeUInt8(ptr, 0x06); // Marker
		return data;
	} else if(m_amfVer == 3) {
		// TODO: AMF 3 support
		return QByteArray();
	}

	// Unknown AMF version
	return QByteArray();
}

QString AMFUndefined::debugString(int indent) const
{
	return QStringLiteral("Undefined");
}
