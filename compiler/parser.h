#pragma once
#include <map>
#include <string>
#include <vector>
#include <prs/string_parser.h>

bool parseFile(std::string path, struct ProtoFile &result);

enum class Type
{
   Invalid,
   Double,
   Float,
   Int32,
   Int64,
   Uint32,
   Uint64,
   Sint32,
   Sint64,
   Fixed32,
   Fixed64,
   Sfixed32,
   Sfixed64,
   Bool,
   String,
   Bytes,
   Message,
   MessagePointer,
   Enum,
   EnumField,
   LookupName
};

struct TypeInfo
{
   Type basicType;
   std::string className;

   static TypeInfo getTypeByName(const std::string &name)
   {
      static const std::map<std::string, Type> nameMap = {
         { "double", Type::Double },
         { "float", Type::Float },
         { "int32", Type::Int32 },
         { "int64", Type::Int64 },
         { "uint32", Type::Uint32 },
         { "uint64", Type::Uint64 },
         { "sint32", Type::Sint32 },
         { "sint64", Type::Sint64 },
         { "fixed32", Type::Fixed32 },
         { "fixed64", Type::Fixed64 },
         { "sfixed32", Type::Sfixed32 },
         { "sfixed64", Type::Sfixed64 },
         { "bool", Type::Bool },
         { "string", Type::String },
         { "bytes", Type::Bytes }
      };

      auto itr = nameMap.find(name);
      if (itr != nameMap.end()) {
         return { itr->second, {} };
      } else {
         return { Type::LookupName, name };
      }
   }
};

enum class FieldRule
{
   None,
   Optional,
   Required,
   Repeated
};

struct FieldOption
{
   std::string name;
   std::string value;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<0>(result).value);
      value = std::move(std::get<2>(result).value);
   }
};

struct Field
{
   FieldRule rule = FieldRule::None;
   TypeInfo type;
   std::string name;
   std::string value;
   std::vector<FieldOption> options;
   std::string nativeName;
   std::string nativeType;
   std::string nativeAbsoluteType;

   template<typename Result>
   void construct(Result &&result)
   {
      if (std::get<0>(result)) {
         auto &ruleStr = *std::get<0>(result);

         if (_strcmpi(ruleStr, "optional") == 0) {
            rule = FieldRule::Optional;
         } else if (_strcmpi(ruleStr, "required") == 0) {
            rule = FieldRule::Required;
         } else if (_strcmpi(ruleStr, "repeated") == 0) {
            rule = FieldRule::Repeated;
         } else {
            rule = FieldRule::None;
         }
      }

      type = TypeInfo::getTypeByName(std::get<1>(result).value);
      name = std::move(std::get<2>(result).value);
      value = std::move(std::get<4>(result).value);

      if (std::get<5>(result)) {
         auto &opts = *std::get<5>(result);

         for (auto &opt : opts.options) {
            FieldOption option;
            option.name = std::move(opt.name);
            option.value = std::move(opt.value);
            options.push_back(std::move(option));
         }
      }
   }
};

struct Enum
{
   std::string name;
   std::string nativeName;
   std::vector<Field> fields;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<1>(result).value);
      auto &values = std::get<3>(result);

      for (auto &value : values) {
         Field field;
         field.type.basicType = Type::EnumField;
         field.name = std::move(value.name);
         field.value = std::move(value.value);

         for (auto &option : value.options.options) {
            field.options.push_back({ std::move(option.name), std::move(option.value) });
         }

         fields.push_back(std::move(field));
      }
   }
};

struct Extend
{
   std::string name;
   std::vector<Field> fields;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<1>(result).value);
      fields = std::move(std::get<3>(result));
   }
};

struct Option
{
   std::string name;
   std::string value;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<1>(result).value);
      value = std::move(std::get<3>(result).value);
   }
};

struct Message
{
   std::string name;
   std::string nativeName;
   std::vector<Option> options;
   std::vector<Field> fields;
   std::vector<Message> messages;
   std::vector<Enum> enums;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<1>(result).value);
      auto &contents = std::get<3>(result);

      for (auto &content : contents) {
         switch (content.which) {
         case 0:
            options.push_back(std::move(*std::get<0>(content)));
            break;
         case 1:
            messages.push_back(std::move(*std::get<1>(content)));
            break;
         case 2:
            enums.push_back(std::move(*std::get<2>(content)));
            break;
         case 3:
            fields.push_back(std::move(*std::get<3>(content)));
            break;
         }
      }
   }
};

struct Import
{
   std::string file;

   template<typename Result>
   void construct(Result &&result)
   {
      file = std::move(std::get<1>(result).value);
      file.erase(file.find_first_of('"'), 1);
      file.erase(file.find_last_of('"'), 1);
   }
};

struct ProtoFile
{
   std::string name;
   std::vector<Option> options;
   std::vector<Enum> enums;
   std::vector<Message> messages;
   std::vector<Import> imports;

   template<typename Result>
   void construct(Result &&result)
   {
      for (auto &&item : result) {
         switch (item.which) {
         case 0:
            enums.push_back(std::move(*std::get<0>(item)));
            break;
         case 1:
            options.push_back(std::move(*std::get<1>(item)));
            break;
         case 2:
            messages.push_back(std::move(*std::get<2>(item)));
            break;
         case 3:
            imports.push_back(std::move(*std::get<3>(item)));
            break;
         }
      }
   }
};

// Temporary to help us translate ast to the proper structs above!
struct ast_stringify
{
   std::string value;

   template<typename Result>
   void construct(Result &&result)
   {
      value = ast_to_string<decltype(value)>(result);
   }
};

struct ast_field_options
{
   std::vector<FieldOption> options;

   template<typename Result>
   void construct(Result &&result)
   {
      options.push_back(std::get<1>(result));
   }
};

struct ast_enum_value
{
   std::string name;
   std::string value;
   ast_field_options options;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<0>(result).value);
      value = std::move(std::get<2>(result).value);

      if (std::get<3>(result)) {
         options = std::move(*std::get<3>(result));
      }
   }
};
