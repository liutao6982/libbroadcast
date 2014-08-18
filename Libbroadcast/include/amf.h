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

#ifndef AMF_H
#define AMF_H

#include "brolog.h"
#include <QtCore/QMap>
#include <QtCore/QString>

// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//*****************************************************************************
// This is not a complete AMF 0/3 specification!
//*****************************************************************************
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

class AMFNumber;
class AMFBoolean;
class AMFString;
class AMFObject;
class AMFEcmaArray;
class AMFNull;
class AMFUndefined;

// Helpers
LBC_EXPORT uint		amfDecodeUInt8(const char *data);
LBC_EXPORT uint		amfDecodeUInt16(const char *data);
LBC_EXPORT uint		amfDecodeUInt24(const char *data);
LBC_EXPORT uint		amfDecodeUInt32(const char *data);
LBC_EXPORT double	amfDecodeDouble(const char *data);
LBC_EXPORT char *	amfEncodeUInt8(char *data, uint val);
LBC_EXPORT char *	amfEncodeUInt16(char *data, uint val);
LBC_EXPORT char *	amfEncodeUInt24(char *data, uint val);
LBC_EXPORT char *	amfEncodeUInt32(char *data, uint val);
LBC_EXPORT char *	amfEncodeDouble(char *data, double val);
LBC_EXPORT char *	amfEncodeUtf8String(char *data, const QByteArray &str);

//=============================================================================
class LBC_EXPORT AMFType
{
public: // Datatypes ----------------------------------------------------------
	enum ValueType {
		UndefinedType = 0,
		NumberType,
		BooleanType,
		StringType,
		ObjectType,
		//MovieClipType,
		NullType,
		//ReferenceType,
		EcmaArrayType,
		//ObjectEndType,
		//StrictArrayType,
		//DateType,
		LongStringType
		//UnsupportedType,
		//RecordSetType,
		//XmlDocumentType,
		//TypedObjectType
	};

protected: // Members ---------------------------------------------------------
	ValueType	m_type;
	int			m_amfVer;

public: // Static methods -----------------------------------------------------
	static uint	decode(const char *data, AMFType **resultOut);

public: // Constructor/destructor ---------------------------------------------
	AMFType(ValueType type);

public: // Methods ------------------------------------------------------------
	ValueType			getAmfType() const;
	void				setAmfVer(int amfVer);
	int					getAmfVer() const;

	virtual QByteArray	serialized() const = 0;
	virtual QString		debugString(int indent = 0) const = 0;

	AMFNumber *				asNumber();
	const AMFNumber *		asNumber() const;
	AMFBoolean *			asBoolean();
	const AMFBoolean *		asBoolean() const;
	AMFString *				asString();
	const AMFString *		asString() const;
	AMFObject *				asObject();
	const AMFObject *		asObject() const;
	AMFEcmaArray *			asEcmaArray();
	const AMFEcmaArray *	asEcmaArray() const;
	AMFNull *				asNull();
	const AMFNull *			asNull() const;
	AMFUndefined *			asUndefined();
	const AMFUndefined *	asUndefined() const;
};
BroLog operator<<(BroLog log, const AMFType &amf);
BroLog operator<<(BroLog log, AMFType *amf);
//=============================================================================

inline AMFType::ValueType AMFType::getAmfType() const
{
	return m_type;
}

inline void AMFType::setAmfVer(int amfVer)
{
	m_amfVer = amfVer;
}

inline int AMFType::getAmfVer() const
{
	return m_amfVer;
}

//=============================================================================
class LBC_EXPORT AMFNumber : public AMFType
{
private: // Members -----------------------------------------------------------
	double	m_value;

public: // Constructor/destructor ---------------------------------------------
	AMFNumber(double value = 0.0);
	AMFNumber(const AMFNumber &other);
	AMFNumber &operator=(const AMFNumber &other);

public: // Methods ------------------------------------------------------------
	void				setValue(double value);
	double				getValue() const;

	virtual QByteArray	serialized() const;
	virtual QString		debugString(int indent = 0) const;
};
//=============================================================================

inline void AMFNumber::setValue(double value)
{
	m_value = value;
}

inline double AMFNumber::getValue() const
{
	return m_value;
}

//=============================================================================
class LBC_EXPORT AMFBoolean : public AMFType
{
private: // Members -----------------------------------------------------------
	bool	m_value;

public: // Constructor/destructor ---------------------------------------------
	AMFBoolean(bool value = false);
	AMFBoolean(const AMFBoolean &other);
	AMFBoolean &operator=(const AMFBoolean &other);

public: // Methods ------------------------------------------------------------
	void				setValue(bool value);
	bool				getValue() const;

	virtual QByteArray	serialized() const;
	virtual QString		debugString(int indent = 0) const;
};
//=============================================================================

inline void AMFBoolean::setValue(bool value)
{
	m_value = value;
}

inline bool AMFBoolean::getValue() const
{
	return m_value;
}

//=============================================================================
/// <summary>
/// Automatically switches between "short" and "long" strings.
/// </summary>
class LBC_EXPORT AMFString : public QString, public AMFType
{
public: // Constructor/destructor ---------------------------------------------
	AMFString();
	AMFString(const QString &str);
	AMFString(const AMFString &other);
	AMFString &operator=(const AMFString &other);

public: // Methods ------------------------------------------------------------
	virtual QByteArray	serialized() const;
	virtual QString		debugString(int indent = 0) const;
};
//=============================================================================

//=============================================================================
/// <summary>
/// An anonymous ActionScript object. Takes memory ownership of all child
/// values. We use a QMap as the order of QHashes change every execution and
/// makes it difficult to test.
/// </summary>
class LBC_EXPORT AMFObject : public QMap<QString, AMFType *>, public AMFType
{
public: // Constructor/destructor ---------------------------------------------
	AMFObject();
	AMFObject(const AMFObject &other);
	AMFObject &operator=(const AMFObject &other);
	~AMFObject();

public: // Methods ------------------------------------------------------------
	void				deepClear();

	virtual QByteArray	serialized() const;
	virtual QString		debugString(int indent = 0) const;
};
//=============================================================================

//=============================================================================
/// <summary>
/// Absolutely identical to an AMFObject for our purposes except its serialized
/// form contains an single additional 32-bit unsigned integer between its
/// marker and the object properties.
/// </summary>
class LBC_EXPORT AMFEcmaArray : public AMFObject
{
private: // Members -----------------------------------------------------------
	uint	m_associativeCount;

public: // Constructor/destructor ---------------------------------------------
	AMFEcmaArray();
	AMFEcmaArray(const AMFEcmaArray &other);
	AMFEcmaArray &operator=(const AMFEcmaArray &other);

public: // Methods ------------------------------------------------------------
	void	setAssociativeCount(uint count);
	uint	getAssociativeCount() const;
};
//=============================================================================

inline void AMFEcmaArray::setAssociativeCount(uint count)
{
	m_associativeCount = count;
}

inline uint AMFEcmaArray::getAssociativeCount() const
{
	return m_associativeCount;
}

//=============================================================================
class LBC_EXPORT AMFNull : public AMFType
{
public: // Constructor/destructor ---------------------------------------------
	AMFNull();
	AMFNull(const AMFNull &other);
	AMFNull &operator=(const AMFNull &other);

public: // Methods ------------------------------------------------------------
	virtual QByteArray	serialized() const;
	virtual QString		debugString(int indent = 0) const;
};
//=============================================================================

//=============================================================================
class LBC_EXPORT AMFUndefined : public AMFType
{
public: // Constructor/destructor ---------------------------------------------
	AMFUndefined();
	AMFUndefined(const AMFUndefined &other);
	AMFUndefined &operator=(const AMFUndefined &other);

public: // Methods ------------------------------------------------------------
	virtual QByteArray	serialized() const;
	virtual QString		debugString(int indent = 0) const;
};
//=============================================================================

#endif // AMF_H
