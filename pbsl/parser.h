#pragma once
#include <cassert>
#include "string_view.h"

namespace pbsl
{

class Parser
{
public:
   struct Tag
   {
      unsigned field;
      unsigned type;
   };

   static const auto TagTypeBits = 3;
   static const auto TagTypeMask = (1 << 3) - 1;

   enum WireType
   {
      VarInt = 0,
      Fixed64 = 1,
      LengthDelimited = 2,
      StartGroup = 3,
      EndGroup = 4,
      Fixed32 = 5
   };

public:
   Parser(const std::string_view &data) :
      mData(reinterpret_cast<ptrdiff_t>(data.data())),
      mSize(data.size()),
      mPosition(0)
   {
   }

   bool eof()
   {
      return mPosition == mSize;
   }

   Tag readTag()
   {
      unsigned field = *reinterpret_cast<uint16_t*>(mData + mPosition);
      unsigned type = field & TagTypeMask;

      // ffff is just padding, set EOF, return 0
      if (field == 0xffff) {
         mPosition = mSize;
         return { 0, 0 };
      }
      
      if (field & 0x80) {
         field = ((field & 0x7f) | ((field & 0xff00) >> 1)) >> TagTypeBits;
         mPosition += 2;
      } else {
         field = (field & 0xff) >> TagTypeBits;
         mPosition += 1;
      }

      return { field, type };
   }

   float readFloat()
   {
      auto value = *reinterpret_cast<float*>(mData + mPosition);
      mPosition += 4;
      return value;
   }

   double readDouble()
   {
      auto value = *reinterpret_cast<double*>(mData + mPosition);
      mPosition += 8;
      return value;
   }

   int32_t readInt32()
   {
      return static_cast<int32_t>(readVarUint32());
   }

   int64_t readInt64()
   {
      return static_cast<int64_t>(readVarUint64());
   }

   uint32_t readUint32()
   {
      return readVarUint32();
   }

   uint64_t readUint64()
   {
      return readVarUint64();
   }

   int32_t readSint32()
   {
      return zigZagDecode32(readVarUint32());
   }

   int64_t readSint64()
   {
      return zigZagDecode64(readVarUint64());
   }

   uint32_t readFixed32()
   {
      auto value = *reinterpret_cast<uint32_t*>(mData + mPosition);
      mPosition += 4;
      return value;
   }

   uint64_t readFixed64()
   {
      auto value = *reinterpret_cast<uint64_t*>(mData + mPosition);
      mPosition += 8;
      return value;
   }

   int32_t readSfixed32()
   {
      auto value = *reinterpret_cast<int32_t*>(mData + mPosition);
      mPosition += 4;
      return value;
   }

   int64_t readSfixed64()
   {
      auto value = *reinterpret_cast<int64_t*>(mData + mPosition);
      mPosition += 8;
      return value;
   }

   bool readBool()
   {
      return !!readVarUint32();
   }

   std::string_view readString()
   {
      auto length = readVarUint32();
      auto value = std::string_view { reinterpret_cast<std::string_view::char_type*>(mData + mPosition), length };
      mPosition += length;
      return value;
   }

   std::string_view readBytes()
   {
      return readString();
   }

   std::string_view readMessage()
   {
      return readString();
   }

   uint32_t readVarUint32()
   {
      auto ptr = reinterpret_cast<const uint8_t*>(mData + mPosition);
      auto value = 0u;
      auto bytes = 0u;

      do {
         value |= (*ptr & 0x7f) << (bytes * 7u);
         bytes++;
      } while (*(ptr++) & 0x80 && bytes <= 5);

      mPosition += bytes;
      return value;
   }

   uint64_t readVarUint64()
   {
      auto ptr = reinterpret_cast<const uint8_t*>(mData + mPosition);
      auto value = 0ull;
      auto bytes = 0u;

      do {
         value |= (*ptr & 0x7f) << (bytes * 7u);
         bytes++;
      } while (*(ptr++) & 0x80 && bytes <= 10);

      mPosition += bytes;
      return value;
   }

   uint32_t zigZagDecode32(uint32_t value)
   {
      return (value >> 1) ^ -static_cast<int32_t>(value & 1);
   }

   uint64_t zigZagDecode64(uint64_t value)
   {
      return (value >> 1) ^ -static_cast<int64_t>(value & 1);
   }

private:
   ptrdiff_t mData;
   size_t mSize;
   size_t mPosition;
};

}