// mtcore_codec.cpp                                                   -*-C++-*-
#include "mtcore_codec.h"

#include <string.h>

namespace mtcore {

                                // ----------------
                                // class ByteWriter
                                // ----------------

ByteWriter::ByteWriter(std::string *buffer)
: d_buffer_p(buffer)
{
}

void ByteWriter::putU8(Uint8 value)
{
    d_buffer_p->push_back((char)value);
}

void ByteWriter::putU16(Uint16 value)
{
    putU8((Uint8)(value >> 8));
    putU8((Uint8)(value));
}

void ByteWriter::putU32(Uint32 value)
{
    putU8((Uint8)(value >> 24));
    putU8((Uint8)(value >> 16));
    putU8((Uint8)(value >> 8));
    putU8((Uint8)(value));
}

void ByteWriter::putU64(Uint64 value)
{
    putU32((Uint32)(value >> 32));
    putU32((Uint32)(value & 0xFFFFFFFF));
}

void ByteWriter::putI64(Int64 value)
{
    putU64((Uint64)value);
}

void ByteWriter::putDouble(double value)
{
    Uint64 bits;
    memcpy(&bits, &value, sizeof(bits));
    putU64(bits);
}

void ByteWriter::putString(const std::string& value)
{
    putU32((Uint32)value.size());
    d_buffer_p->append(value);
}

void ByteWriter::putBytes(const char *data, size_t length)
{
    d_buffer_p->append(data, length);
}

void ByteWriter::patchU32(size_t offset, Uint32 value)
{
    (*d_buffer_p)[offset]     = (char)(value >> 24);
    (*d_buffer_p)[offset + 1] = (char)(value >> 16);
    (*d_buffer_p)[offset + 2] = (char)(value >> 8);
    (*d_buffer_p)[offset + 3] = (char)(value);
}

size_t ByteWriter::size() const
{
    return d_buffer_p->size();
}

                                // ----------------
                                // class ByteReader
                                // ----------------

ByteReader::ByteReader(const char *data, size_t length)
: d_data_p(data)
, d_length(length)
, d_position(0)
, d_valid(true)
{
}

Uint8 ByteReader::getU8()
{
    if (d_position + 1 > d_length) {
        d_valid = false;
        return 0;
    }
    return (Uint8)d_data_p[d_position++];
}

Uint16 ByteReader::getU16()
{
    Uint16 hi = getU8();
    Uint16 lo = getU8();
    return (Uint16)((hi << 8) | lo);
}

Uint32 ByteReader::getU32()
{
    Uint32 b0 = getU8();
    Uint32 b1 = getU8();
    Uint32 b2 = getU8();
    Uint32 b3 = getU8();
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

Uint64 ByteReader::getU64()
{
    Uint64 hi = getU32();
    Uint64 lo = getU32();
    return (hi << 32) | lo;
}

Int64 ByteReader::getI64()
{
    return (Int64)getU64();
}

double ByteReader::getDouble()
{
    Uint64 bits = getU64();
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string ByteReader::getString()
{
    Uint32 length = getU32();
    if (!d_valid || d_position + length > d_length) {
        d_valid = false;
        return std::string();
    }
    std::string result(d_data_p + d_position, length);
    d_position += length;
    return result;
}

bool ByteReader::isValid() const
{
    return d_valid;
}

size_t ByteReader::remaining() const
{
    return d_length - d_position;
}

                                // ------------------
                                // class MessageCodec
                                // ------------------

namespace {

void encodeElement(ByteWriter *writer, const Element& element)
{
    writer->putString(element.name());
    writer->putU8((Uint8)element.type());
    switch (element.type()) {
      case DataType::e_NONE:
        break;
      case DataType::e_BOOL:
        writer->putU8(element.getBool() ? 1 : 0);
        break;
      case DataType::e_INT64:
      case DataType::e_DATETIME:
        writer->putI64(element.getInt64());
        break;
      case DataType::e_FLOAT64:
        writer->putDouble(element.getFloat64());
        break;
      case DataType::e_STRING:
        writer->putString(element.getString());
        break;
      case DataType::e_SEQUENCE:
      case DataType::e_ARRAY: {
        writer->putU32((Uint32)element.numChildren());
        for (size_t i = 0; i < element.numChildren(); ++i) {
            encodeElement(writer, *element.childAt(i));
        }
      } break;
    }
}

int decodeElement(Element *element, ByteReader *reader, int depth)
{
    if (depth > MessageCodec::k_MAX_DEPTH) {
        return -1;
    }
    element->clear();
    element->setName(reader->getString());
    Uint8 type = reader->getU8();
    switch (type) {
      case DataType::e_NONE:
        break;
      case DataType::e_BOOL:
        element->setBool(reader->getU8() != 0);
        break;
      case DataType::e_INT64:
        element->setInt64(reader->getI64());
        break;
      case DataType::e_DATETIME:
        element->setDatetime(reader->getI64());
        break;
      case DataType::e_FLOAT64:
        element->setFloat64(reader->getDouble());
        break;
      case DataType::e_STRING:
        element->setString(reader->getString());
        break;
      case DataType::e_SEQUENCE:
      case DataType::e_ARRAY: {
        Uint32 count = reader->getU32();
        if (!reader->isValid() || count > reader->remaining()) {
            return -2;
        }
        element->setArray(type == DataType::e_ARRAY);
        for (Uint32 i = 0; i < count; ++i) {
            Element child;
            if (decodeElement(&child, reader, depth + 1) != 0) {
                return -3;
            }
            Element *slot = type == DataType::e_ARRAY
                          ? element->appendItem()
                          : element->addChild(child.name());
            *slot = child;
        }
      } break;
      default:
        return -4;
    }
    return reader->isValid() ? 0 : -5;
}

}  // close unnamed namespace

void MessageCodec::encode(std::string *frame, const Message& message)
{
    frame->clear();
    frame->reserve(128);
    ByteWriter writer(frame);
    writer.putU32(0);  // patched below
    writer.putU16((Uint16)k_MAGIC);
    writer.putU8((Uint8)k_VERSION);
    writer.putU16((Uint16)message.type());
    writer.putU64(message.correlationId());
    writer.putU32(message.flags());
    writer.putString(message.service());
    writer.putString(message.operation());
    writer.putString(message.topic());
    encodeElement(&writer, message.body());
    writer.patchU32(0, (Uint32)(frame->size() - k_HEADER_SIZE));
}

int MessageCodec::decode(Message *message, const char *payload, size_t length)
{
    ByteReader reader(payload, length);
    if (reader.getU16() != k_MAGIC) {
        return -1;
    }
    if (reader.getU8() != k_VERSION) {
        return -2;
    }
    Uint16 type = reader.getU16();
    if (!MessageType::isValid(type)) {
        return -3;
    }
    message->reset();
    message->setType((MessageType::Value)type);
    message->setCorrelationId(reader.getU64());
    message->setFlags(reader.getU32());
    message->setService(reader.getString());
    message->setOperation(reader.getString());
    message->setTopic(reader.getString());
    if (!reader.isValid()) {
        return -4;
    }
    if (decodeElement(&message->body(), &reader, 0) != 0) {
        return -5;
    }
    return reader.remaining() == 0 ? 0 : -6;
}

int MessageCodec::frameLength(Uint32 *length, const char *header)
{
    ByteReader reader(header, k_HEADER_SIZE);
    Uint32 value = reader.getU32();
    if (value < 3 || value > (Uint32)k_MAX_FRAME) {
        return -1;
    }
    *length = value;
    return 0;
}

}  // close namespace mtcore
