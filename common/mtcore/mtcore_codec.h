// mtcore_codec.h                                                     -*-C++-*-
#ifndef INCLUDED_MTCORE_CODEC
#define INCLUDED_MTCORE_CODEC

//@PURPOSE: Provide the binary wire encoding for 'Message' frames.
//
//@CLASSES:
//  mtcore::ByteWriter:    appends big-endian primitives to a buffer
//  mtcore::ByteReader:    reads big-endian primitives with bounds checking
//  mtcore::MessageCodec:  frame <-> 'Message'
//
//@DESCRIPTION: A frame is:
//..
//  +---------+---------+---------+-------------------------------+
//  | len u32 | magic   | version |  payload (len - 3 bytes)      |
//  |         | u16 'MT'| u8 = 1  |                               |
//  +---------+---------+---------+-------------------------------+
//  payload  := type u16, cid u64, flags u32,
//              service str, operation str, topic str, element
//  element  := name str, type u8, value
//  value    := BOOL u8 | INT64 i64 | FLOAT64 f64 | STRING str |
//              DATETIME i64 | SEQUENCE/ARRAY count u32, element*
//  str      := length u32, bytes
//..
// All integers are big-endian.  See docs/PROTOCOL.md.

#include "mtcore_message.h"
#include "mtcore_platform.h"

#include <string>
#include <vector>

namespace mtcore {

class ByteWriter {
    std::string *d_buffer_p;  // held, not owned

  public:
    explicit ByteWriter(std::string *buffer);

    void putU8(Uint8 value);
    void putU16(Uint16 value);
    void putU32(Uint32 value);
    void putU64(Uint64 value);
    void putI64(Int64 value);
    void putDouble(double value);
    void putString(const std::string& value);
    void putBytes(const char *data, size_t length);
    void patchU32(size_t offset, Uint32 value);
    size_t size() const;
};

class ByteReader {
    const char *d_data_p;
    size_t      d_length;
    size_t      d_position;
    bool        d_valid;

  public:
    ByteReader(const char *data, size_t length);

    Uint8 getU8();
    Uint16 getU16();
    Uint32 getU32();
    Uint64 getU64();
    Int64 getI64();
    double getDouble();
    std::string getString();

    bool isValid() const;
        // 'false' once any read ran past the end of the buffer.
    size_t remaining() const;
};

struct MessageCodec {
    enum {
        k_HEADER_SIZE   = 4,
        k_MAGIC         = 0x4D54,   // "MT"
        k_VERSION       = 1,
        k_MAX_FRAME     = 16 * 1024 * 1024,
        k_MAX_DEPTH     = 32
    };

    static void encode(std::string *frame, const Message& message);
        // Replace 'frame' with the full frame for 'message', including the
        // 4-byte length prefix.

    static int decode(Message *message, const char *payload, size_t length);
        // Decode a frame body (everything after the length prefix).  Return
        // 0 on success and a non-zero value if the payload is malformed.

    static int frameLength(Uint32 *length, const char *header);
        // Read the length prefix from the first 4 bytes of 'header'.
        // Return non-zero if the length is out of range.
};

}  // close namespace mtcore

#endif
