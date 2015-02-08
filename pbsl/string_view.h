#pragma once
#include <cstddef>
#include <stdint.h>

namespace std
{

class string_view
{
public:
   using char_type = char;

public:
   string_view() :
      mData(nullptr),
      mSize(0)
   {
   }

   string_view(const char_type *data, size_t size) :
      mData(data),
      mSize(size)
   {
   }

   char_type const *data() const
   {
      return mData;
   }

   size_t size() const
   {
      return mSize;
   }

   std::string toString() const
   {
      return std::string(mData, mSize);
   }

   operator std::string() const
   {
      return toString();
   }

private:
   const char_type *mData;
   size_t mSize;
};

};
