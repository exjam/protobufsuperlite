#pragma once

#include "string_view.h"

namespace pbsl
{

class parser
{
public:
   parser(const string_view &data) :
      mData(data)
   {
   }

private:
   const string_view &mData;
   size_t mPosition;
};

}