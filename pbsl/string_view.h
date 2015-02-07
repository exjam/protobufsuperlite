#pragma once
#include <cstddef>

namespace std
{

class string_view
{
public:
   using value_type = char;

public:
   string_view(value_type const *data, size_t size) :
      mData(data),
      mSize(size)
   {
   }

   value_type const *data() const
   {
      return mData;
   }

   size_t size() const
   {
      return mSize;
   }

private:
   value_type const *mData;
   size_t mSize;
};

};
