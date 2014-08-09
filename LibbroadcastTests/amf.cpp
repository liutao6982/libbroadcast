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

TEST(AMF0Test, EncodeNumberZero)
{
	AMFNumber val(0.0);
	ASSERT_EQ(0.0, val.getValue());
	QByteArray data = val.serialized();
	const char expected[] = {
		0x00, // Marker
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	ASSERT_EQ(sizeof(expected), data.size());
	for(int i = 0; i < data.size(); i++)
		ASSERT_EQ(expected[i], data[i]);
}

TEST(AMF0Test, DecodeNumberZero)
{
	AMFNumber val(0.0);
	ASSERT_EQ(0.0, val.getValue());
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFNumber *outVal = out->asNumber();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(9, outSize);
	EXPECT_EQ(val.getValue(), outVal->getValue());

	delete out;
}

TEST(AMF0Test, EncodeNumberNonzero)
{
	AMFNumber val(854.0);
	ASSERT_EQ(854.0, val.getValue());
	QByteArray data = val.serialized();
	const uchar expected[] = {
		0x00, // Marker
		0x40, 0x8A, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	ASSERT_EQ(sizeof(expected), data.size());
	for(int i = 0; i < data.size(); i++)
		ASSERT_EQ((char)expected[i], data[i]);
}

TEST(AMF0Test, DecodeNumberNonzero)
{
	AMFNumber val(854.0);
	ASSERT_EQ(854.0, val.getValue());
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFNumber *outVal = out->asNumber();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(9, outSize);
	EXPECT_EQ(val.getValue(), outVal->getValue());

	delete out;
}

TEST(AMF0Test, EncodeBooleanFalse)
{
	AMFBoolean val(false);
	QByteArray data = val.serialized();
	const char expected[] = {
		0x01, // Marker
		0x00
	};
	ASSERT_EQ(sizeof(expected), data.size());
	for(int i = 0; i < data.size(); i++)
		ASSERT_EQ(expected[i], data[i]);
}

TEST(AMF0Test, DecodeBooleanFalse)
{
	AMFBoolean val(false);
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFBoolean *outVal = out->asBoolean();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(2, outSize);
	EXPECT_EQ(val.getValue(), outVal->getValue());

	delete out;
}

TEST(AMF0Test, EncodeBooleanTrue)
{
	AMFBoolean val(true);
	QByteArray data = val.serialized();
	ASSERT_EQ(2, data.size());
	ASSERT_EQ(0x01, data[0]); // Marker
	ASSERT_NE(0x00, data[1]);
}

TEST(AMF0Test, DecodeBooleanTrue)
{
	AMFBoolean val(true);
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFBoolean *outVal = out->asBoolean();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(2, outSize);
	EXPECT_EQ(val.getValue(), outVal->getValue());

	delete out;
}

TEST(AMF0Test, EncodeStringEmpty)
{
	AMFString val;
	QByteArray data = val.serialized();
	const char expected[] = {
		0x02, // Marker
		0x00, 0x00 // Length
	};
	ASSERT_EQ(sizeof(expected), data.size());
	for(int i = 0; i < data.size(); i++)
		ASSERT_EQ(expected[i], data[i]);
}

TEST(AMF0Test, DecodeStringEmpty)
{
	AMFString val;
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFString *outVal = out->asString();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(3, outSize);
	EXPECT_TRUE(outVal->isEmpty());

	delete out;
}

TEST(AMF0Test, EncodeStringShortAscii)
{
	AMFString val(QStringLiteral("FMS/3,0,1,123"));
	QByteArray data = val.serialized();
	const char expected[] = {
		0x02, // Marker
		0x00, 0x0D, // Length
		0x46, 0x4D, 0x53, 0x2F, 0x33, 0x2C, 0x30, 0x2C,
		0x31, 0x2C, 0x31, 0x32, 0x33
	};
	ASSERT_EQ(sizeof(expected), data.size());
	for(int i = 0; i < data.size(); i++)
		ASSERT_EQ(expected[i], data[i]);
}

TEST(AMF0Test, DecodeStringShortAscii)
{
	AMFString val(QStringLiteral("FMS/3,0,1,123"));
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFString *outVal = out->asString();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(3 + 13, outSize);
	EXPECT_EQ(val, *outVal);

	delete out;
}

TEST(AMF0Test, DecodeStringLongUtf8)
{
	// Generate long (>65535 bytes) UTF-8 string
	QString str =
		QStringLiteral("\xE3\x81\x82 \xE3\x81\x84 "); // Japanese A and I
	str.append(QString(21845, QChar(0x3046))); // Japanese U
	str.append(QStringLiteral(" \xE3\x81\x88")); // Japanese E
	uint utf8Len = str.toUtf8().length();
	ASSERT_LT(65535U, utf8Len);

	AMFString val(str);
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFString *outVal = out->asString();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(5 + utf8Len, outSize);
	EXPECT_EQ(val, *outVal);

	delete out;
}

TEST(AMF0Test, EncodeObjectEmpty)
{
	AMFObject val;
	QByteArray data = val.serialized();
	const char expected[] = {
		0x03, // Marker
		0x00, 0x00,
		0x09 // End marker
	};
	ASSERT_EQ(sizeof(expected), data.size());
	for(int i = 0; i < data.size(); i++)
		ASSERT_EQ(expected[i], data[i]);
}

TEST(AMF0Test, DecodeObjectEmpty)
{
	AMFObject val;
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFObject *outVal = out->asObject();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(4, outSize);
	EXPECT_TRUE(outVal->isEmpty());

	delete out;
}

TEST(AMF0Test, EncodeObject)
{
	AMFObject val;
	val["capabilities"] = new AMFNumber(31.0);
	val["fmsVer"] = new AMFString("FMS/3,0,1,123");

	QByteArray data = val.serialized();
	const char expected[] = {
		0x03, // Marker

		// Property 1 key: "capabilities"
		0x00, 0x0C, // Length
		0x63, 0x61, 0x70, 0x61, 0x62, 0x69, 0x6C, 0x69,
		0x74, 0x69, 0x65, 0x73,

		// Property 1 value: Number
		0x00, // Marker
		0x40, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		// Property 2 key: "fmsVer"
		0x00, 0x06, // Length
		0x66, 0x6D, 0x73, 0x56, 0x65, 0x72,

		// Property 3 value: String
		0x02, // Marker
		0x00, 0x0D, // Length
		0x46, 0x4D, 0x53, 0x2F, 0x33, 0x2C, 0x30, 0x2C,
		0x31, 0x2C, 0x31, 0x32, 0x33,

		0x00, 0x00,
		0x09 // End marker
	};
	ASSERT_EQ(sizeof(expected), data.size());
	for(int i = 0; i < data.size(); i++)
		ASSERT_EQ(expected[i], data[i]);
}

TEST(AMF0Test, DecodeObject)
{
	AMFObject val;
	val["capabilities"] = new AMFNumber(31.0);
	val["fmsVer"] = new AMFString("FMS/3,0,1,123");

	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFObject *outVal = out->asObject();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(51, outSize);
	EXPECT_EQ(2, outVal->count());

	EXPECT_TRUE(outVal->contains("capabilities"));
	AMFNumber *outNum = outVal->value("capabilities")->asNumber();
	ASSERT_FALSE(outNum == NULL);
	EXPECT_EQ(31.0, outNum->getValue());

	EXPECT_TRUE(outVal->contains("fmsVer"));
	AMFString *outStr = outVal->value("fmsVer")->asString();
	ASSERT_FALSE(outStr == NULL);
	EXPECT_EQ(QString("FMS/3,0,1,123"), outStr);

	delete out;
}

TEST(AMF0Test, EncodeEcmaArray)
{
	AMFEcmaArray val;
	val.setAssociativeCount(0x1234);
	QByteArray data = val.serialized();
	const char expected[] = {
		0x08, // Marker
		0x00, 0x00, 0x12, 0x34,
		0x00, 0x00,
		0x09 // End marker
	};
	ASSERT_EQ(sizeof(expected), data.size());
	for(int i = 0; i < data.size(); i++)
		ASSERT_EQ(expected[i], data[i]);
}

TEST(AMF0Test, DecodeEcmaArray)
{
	AMFEcmaArray val;
	val.setAssociativeCount(0x1234);
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFEcmaArray *outVal = out->asEcmaArray();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(8, outSize);
	EXPECT_TRUE(outVal->isEmpty());
	EXPECT_EQ(val.getAssociativeCount(), outVal->getAssociativeCount());

	delete out;
}

TEST(AMF0Test, EncodeNull)
{
	AMFNull val;
	QByteArray data = val.serialized();
	ASSERT_EQ(1, data.size());
	ASSERT_EQ(0x05, data[0]); // Marker
}

TEST(AMF0Test, DecodeNull)
{
	AMFNull val;
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFNull *outVal = out->asNull();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(1, outSize);

	delete out;
}

TEST(AMF0Test, EncodeUndefined)
{
	AMFUndefined val;
	QByteArray data = val.serialized();
	ASSERT_EQ(1, data.size());
	ASSERT_EQ(0x06, data[0]); // Marker
}

TEST(AMF0Test, DecodeUndefined)
{
	AMFUndefined val;
	QByteArray data = val.serialized();
	AMFType *out = NULL;
	uint outSize = AMFType::decode(data.constData(), &out);
	AMFUndefined *outVal = out->asUndefined();

	ASSERT_FALSE(outVal == NULL);
	EXPECT_EQ(1, outSize);

	delete out;
}
