#include <fstream>
#include <sstream>
#include <cassert>
#include <string>
#include <vector>
#include <cctype>
#include <map>
#include <iostream>
#include <prs\parser.h>

using namespace prs;

struct ast_symbol
{
   std::string value;

   template<typename Result>
   void construct(Result &&result)
   {
      value = ast_to_string<decltype(value)>(result);
   }
};

struct ast_number
{
   std::string value;

   template<typename Result>
   void construct(Result &&result)
   {
      value = ast_to_string<decltype(value)>(result);
   }
};

struct ast_string
{
   std::string value;

   template<typename Result>
   void construct(Result &&result)
   {
      value = ast_to_string<decltype(value)>(std::get<1>(result));
   }
};

struct ast_value
{
   int type;
   std::string value;

   template<typename Result>
   void construct(Result &&result)
   {
      type = result.which;

      switch(type) {
      case 0:
         value = std::move((*std::get<0>(result)).value);
         break;
      case 1:
         value = std::move((*std::get<1>(result)).value);
         break;
      case 2:
         value = std::move((*std::get<2>(result)).value);
         break;
      }
   }
};

struct ast_field_option_value
{
   std::string name;
   ast_value value;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<0>(result).value);
      value = std::get<2>(result);
   }
};

struct ast_field_options
{
   std::vector<ast_field_option_value> options;

   template<typename Result>
   void construct(Result &&result)
   {
      options.push_back(std::get<1>(result));
   }
};

struct ast_enum_value
{
   std::string name;
   ast_value value;
   ast_field_options options;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<0>(result).value);
      value = std::get<2>(result);

      if (std::get<3>(result)) {
         options = std::move(*std::get<3>(result));
      }
   }
};

struct ast_enum
{
   std::string name;
   std::vector<ast_enum_value> values;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<1>(result).value);
      values = std::get<3>(result);
   }
};

struct ast_option
{
   std::string name;
   ast_value value;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<1>(result).value);
      value = std::get<3>(result);
   }
};

struct ast_property
{
   std::string field_rule;
   std::string type;
   std::string name;
   std::string index;
   ast_field_options options;

   template<typename Result>
   void construct(Result &&result)
   {
      if (std::get<0>(result)) {
         field_rule = std::move(*std::get<0>(result));
      }

      type = std::move(std::get<1>(result).value);
      name = std::move(std::get<2>(result).value);
      index = std::move(std::get<4>(result).value);

      if (std::get<5>(result)) {
         options = std::move(*std::get<5>(result));
      }
   }
};

struct ast_extend
{
   std::string name;
   std::vector<ast_property> properties;

   template<typename Result>
   void construct(Result &&result)
   {
      name = std::move(std::get<1>(result).value);
      properties = std::move(std::get<3>(result));
   }
};

struct ast_message
{
   std::string name;
   std::vector<ast_option> options;
   std::vector<ast_property> properties;
   std::vector<ast_message> messages;
   std::vector<ast_enum> enums;

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
            properties.push_back(std::move(*std::get<3>(content)));
            break;
         }
      }
   }
};

struct ast_import
{
   std::string file;

   template<typename Result>
   void construct(Result &&result)
   {
      file = std::move(std::get<1>(result).value);
   }
};

struct ast_proto
{
   std::vector<ast_option> options;
   std::vector<ast_enum> enums;
   std::vector<ast_message> messages;
   std::vector<ast_import> imports;

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

// Basic Types
auto letter = char_range('a', 'z') | char_range('A', 'Z');
auto digit = char_range('0', '9');
auto whitespace = atomic(*(char_(' ') | char_('\t') | char_('\n') | char_('\r')));

struct ParseContext
{
   decltype(whitespace) whitespace_parser;
};

template<typename AstType> using parse_rule = rule<AstType, std::string::iterator, ParseContext>;

// "string"
parse_rule<ast_string> string_rule;
auto string_def =
   atomic(char_('"') >> +(!char_('"') >> char_any()) >> char_('"'));

// float number -0.5
parse_rule<ast_number> number_rule;
auto number_def =
   atomic(-char_('-') >> +digit >> -(char_('.') >> +digit));

// symbol / identifier
parse_rule<ast_symbol> symbol_rule;
auto symbol_def =
   atomic(+(letter | digit | char_('_') | char_('.') | char_('(') | char_(')')));

// field_value
auto field_value = ast<ast_value>() >>
   (number_rule | symbol_rule | string_rule);

// TODO: Support multiple options
// field_option
parse_rule<ast_field_options> field_options_rule;
auto field_option_value = ast<ast_field_option_value>() >>
   (symbol_rule >> char_('=') >> field_value);

auto field_options_def =
   char_('[') >>
   field_option_value >>
   char_(']');

// property
parse_rule<ast_property> property_rule;
auto field_flag = (string_("optional") | string_("repeated") | string_("required"));
auto property_def = 
   -field_flag >> // optional | repeated | required
   symbol_rule >> // type
   symbol_rule >> // name
   char_('=') >>
   number_rule >> // value
   -field_options_rule >> // [option = value]
   char_(';');

// option
parse_rule<ast_option> option_rule;
auto option_def =
   string_("option") >>
   symbol_rule >> // name
   char_('=') >>
   field_value >> // value
   char_(';');

// enum
parse_rule<ast_enum> enum_rule;
auto enum_field = ast<ast_enum_value>() >> (
   symbol_rule >> // name
   char_('=') >>
   field_value >> // value
   -field_options_rule >> // [option = value]
   char_(';'));

auto enum_def =
   string_("enum") >>
   symbol_rule >> // name
   char_('{') >>
   +enum_field >> // values
   char_('}');

// import
parse_rule<ast_import> import_rule;
auto import_def =
   string_("import") >>
   string_rule >> // "path"
   char_(';');

// message
parse_rule<ast_message> message_rule;
auto message_def =
   string_("message") >>
   symbol_rule >> // name
   char_('{') >>
   *(option_rule   // option
   | message_rule  // child message
   | enum_rule     // child enum
   | property_rule) >> // property
   char_('}');

// extend
parse_rule<ast_extend> extend_rule;
auto extend_def =
   string_("extend") >>
   symbol_rule >> // name
   char_('{') >>
   *(property_rule) >> // properties
   char_('}');

// .proto parser
parse_rule<ast_proto> proto_parser;
auto proto_def =
   *(enum_rule
   | option_rule
   | message_rule
   | import_rule
   | extend_rule);

std::string ReplaceString(std::string subject, const std::string& search, const std::string& replace)
{
   size_t pos = 0;

   while ((pos = subject.find(search, pos)) != std::string::npos) {
      subject.replace(pos, search.length(), replace);
      pos += replace.length();
   }

   return subject;
}

enum class ProtoBufType
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
   Message
};

static const std::map<ProtoBufType, std::string> NativeTypeMap = {
   { ProtoBufType::Double, "double" },
   { ProtoBufType::Float, "float" },
   { ProtoBufType::Int32, "int32_t" },
   { ProtoBufType::Int64, "int64_t" },
   { ProtoBufType::Uint32, "uint32_t" },
   { ProtoBufType::Uint64, "uint64_t" },
   { ProtoBufType::Sint32, "int32_t" },
   { ProtoBufType::Sint64, "int64_t" },
   { ProtoBufType::Fixed32, "uint32_t" },
   { ProtoBufType::Fixed64, "uint64_t" },
   { ProtoBufType::Sfixed32, "int32_t" },
   { ProtoBufType::Sfixed64, "int64_t" },
   { ProtoBufType::Bool, "bool" },
   { ProtoBufType::String, "std::string_view" },
   { ProtoBufType::Bytes, "std::string_view" }
};

static const std::map<std::string, ProtoBufType> TypeNameMap = {
   { "double", ProtoBufType::Double },
   { "float", ProtoBufType::Float },
   { "int32", ProtoBufType::Int32 },
   { "int64", ProtoBufType::Int64 },
   { "uint32", ProtoBufType::Uint32 },
   { "uint64", ProtoBufType::Uint64 },
   { "sint32", ProtoBufType::Sint32 },
   { "sint64", ProtoBufType::Sint64 },
   { "fixed32", ProtoBufType::Fixed32 },
   { "fixed64", ProtoBufType::Fixed64 },
   { "sfixed32", ProtoBufType::Sfixed32 },
   { "sfixed64", ProtoBufType::Sfixed64 },
   { "bool", ProtoBufType::Bool },
   { "string", ProtoBufType::String },
   { "bytes", ProtoBufType::Bytes }
};

std::string getPropertyType(std::string parentName, ast_property &prop)
{
   bool repeated = false;
   std::string type;

   if (prop.field_rule.compare("repeated") == 0) {
      repeated = true;
   }

   if (repeated) {
      type += "std::vector<";
   }

   auto itr = TypeNameMap.find(prop.type);

   if (itr != TypeNameMap.end()) {
      auto itr2 = NativeTypeMap.find(itr->second);
      assert(itr2 != NativeTypeMap.end());
      type += itr2->second;
   } else {
      std::string propType = prop.type;
      size_t pos;
      pos = prop.type.find(parentName + ".");

      if (pos != std::string::npos) {
         propType = prop.type.substr(pos + parentName.length() + 1);
      } else {
         propType = prop.type;
      }

      propType = ReplaceString(propType, ".", "::");

      if (propType.find("::") == 0) {
         type += propType.substr(2);
      } else {
         type += propType;
      }
   }

   if (repeated) {
      type += ">";
   }

   return type;
}

std::string IndentOnce = "   ";

void addIndent(std::string &indent)
{
   indent += IndentOnce;
}

void subIndent(std::string &indent)
{
   indent.resize(indent.size() - 3);
}

void dumpEnum(std::ostream &out, ast_enum &enum_, std::string indent = "")
{
   out << indent << "enum " << enum_.name << " {" << std::endl;
   addIndent(indent);

   for (ast_enum_value &value : enum_.values) {
      out << indent << value.name << " = " << value.value.value << "," << std::endl;
   }

   subIndent(indent);
   out << indent  << "};" << std::endl << std::endl;
}

void dumpMessageDeclaration(std::ostream &out, ast_message &msg, std::string indent = "")
{
   // Dump message struct
   out << indent << "struct " << msg.name << std::endl;
   out << indent << " {" << std::endl;
   addIndent(indent);

   // Dump child enums
   for (ast_enum &enum_ : msg.enums) {
      dumpEnum(out, enum_, indent);
   }

   // Dump child messages
   for (ast_message &submsg : msg.messages) {
      dumpMessageDeclaration(out, submsg, indent);
   }

   // Dump properties
   for (ast_property &prop : msg.properties) {
      out << indent << getPropertyType(msg.name, prop) << " " << prop.name << ";" << std::endl;
   }

   out << indent << "bool parse(const std::string_view &data);" << std::endl;
   subIndent(indent);
   out << indent << "};" << std::endl << std::endl;
}

enum WireType
{
   VarInt = 0,
   Fixed64 = 1,
   LengthDelimited = 2,
   StartGroup = 3,
   EndGroup = 4,
   Fixed32 = 5
};

void dumpMessageDefinition(std::ostream &out, ast_message &msg, std::string indent = "")
{
   out << indent << "bool " << msg.name << "::parse(const std::string_view &data)" << std::endl;
   out << indent << "{" << std::endl;
   addIndent(indent);

   out << indent << "auto parser = pbsl::parser { data };" << std::endl;
   out << std::endl;

   out << indent << "while(!parser.eof) {" << std::endl;
   addIndent(indent);

   out << indent << "auto tag = parser.readTag();" << std::endl;
   out << std::endl;
   out << indent << "switch(tag.field) {" << std::endl;
   for (auto &prop : msg.properties) {
      out << indent << "case " << prop.index << ":" << std::endl;
      addIndent(indent);
      
      auto itr = TypeNameMap.find(prop.type);
      auto type = ProtoBufType::Message;

      if (itr != TypeNameMap.end()) {
         type = itr->second;
      }

      switch (type) {
      case ProtoBufType::Double:
         out << indent << "assert(tag.type == ProtobufParser::WireType::Fixed64);" << std::endl;
         out << indent << prop.name << " = parser.readDouble();" << std::endl;
         break;
      case ProtoBufType::Float:
         out << indent << "assert(tag.type == ProtobufParser::WireType::Fixed32);" << std::endl;
         out << indent << prop.name << " = parser.readFloat();" << std::endl;
         break;
      case ProtoBufType::Int32:
         out << indent << "assert(tag.type == ProtobufParser::WireType::VarInt);" << std::endl;
         out << indent << prop.name << " = parser.readInt32();" << std::endl;
         break;
      case ProtoBufType::Int64:
         out << indent << "assert(tag.type == ProtobufParser::WireType::VarInt);" << std::endl;
         out << indent << prop.name << " = parser.readInt64();" << std::endl;
         break;
      case ProtoBufType::Uint32:
         out << indent << "assert(tag.type == ProtobufParser::WireType::VarInt);" << std::endl;
         out << indent << prop.name << " = parser.readUint32();" << std::endl;
         break;
      case ProtoBufType::Uint64:
         out << indent << "assert(tag.type == ProtobufParser::WireType::VarInt);" << std::endl;
         out << indent << prop.name << " = parser.readUint64();" << std::endl;
         break;
      case ProtoBufType::Sint32:
         out << indent << "assert(tag.type == ProtobufParser::WireType::VarInt);" << std::endl;
         out << indent << prop.name << " = parser.readSint32();" << std::endl;
         break;
      case ProtoBufType::Sint64:
         out << indent << "assert(tag.type == ProtobufParser::WireType::VarInt);" << std::endl;
         out << indent << prop.name << " = parser.readSint64();" << std::endl;
         break;
      case ProtoBufType::Fixed32:
         out << indent << "assert(tag.type == ProtobufParser::WireType::Fixed32);" << std::endl;
         out << indent << prop.name << " = parser.readFixed32();" << std::endl;
         break;
      case ProtoBufType::Fixed64:
         out << indent << "assert(tag.type == ProtobufParser::WireType::Fixed64);" << std::endl;
         out << indent << prop.name << " = parser.readFixed64();" << std::endl;
         break;
      case ProtoBufType::Sfixed32:
         out << indent << "assert(tag.type == ProtobufParser::WireType::Fixed32);" << std::endl;
         out << indent << prop.name << " = parser.readSfixed32();" << std::endl;
         break;
      case ProtoBufType::Sfixed64:
         out << indent << "assert(tag.type == ProtobufParser::WireType::Fixed64);" << std::endl;
         out << indent << prop.name << " = parser.readSfixed64();" << std::endl;
         break;
      case ProtoBufType::Bool:
         out << indent << "assert(tag.type == ProtobufParser::WireType::VarInt);" << std::endl;
         out << indent << prop.name << " = parser.readBool();" << std::endl;
         break;
      case ProtoBufType::String:
         out << indent << "assert(tag.type == ProtobufParser::WireType::LengthDelimited);" << std::endl;
         out << indent << prop.name << " = parser.readString();" << std::endl;
         break;
      case ProtoBufType::Bytes:
         out << indent << "assert(tag.type == ProtobufParser::WireType::LengthDelimited);" << std::endl;
         out << indent << prop.name << " = parser.readBytes();" << std::endl;
         break;
      case ProtoBufType::Message:
         out << indent << "assert(tag.type == ProtobufParser::WireType::LengthDelimited);" << std::endl;
         out << indent << prop.name << ".parse(parser.readMessage());" << std::endl;
         break;
      }

      out << indent << "break;" << std::endl;
      subIndent(indent);
   }

   out << indent << "default:" << std::endl;
   addIndent(indent);
   out << indent << "assert(0 && \"Invalid field number\");" << std::endl;
   subIndent(indent);
   out << indent << "}" << std::endl;
   
   subIndent(indent);
   out << indent << "}" << std::endl;

   out << std::endl;
   out << indent << "return true;" << std::endl;
   subIndent(indent);
   out << indent << "};" << std::endl << std::endl;
}

int main(int argc, char **argv)
{
   message_rule = message_def;
   property_rule = property_def;
   enum_rule = enum_def;
   field_options_rule = field_options_def;
   import_rule = import_def;
   option_rule = option_def;
   extend_rule = extend_def;
   number_rule = number_def;
   string_rule = string_def;
   proto_parser = proto_def;
   symbol_rule = symbol_def;

   for (int i = 1; i < argc; ++i) {
      // TODO: Flags? nah lol
      std::string path = argv[i];
      std::fstream in;
      std::string file;
      in.open(path, std::fstream::in);

      if (!in.is_open()) {
         std::cout << "Could not open " << path << " for reading" << std::endl;
         return false;
      }

      /* Read file and remove all comments */
      for (std::string line; std::getline(in, line);) {
         auto p = line.rfind("//");

         if (p != std::string::npos) {
            file.append(line.begin(), line.begin() + p);
         } else {
            file.append(line.begin(), line.end());
         }

         file += '\n';
      }

      in.close();

      ParseContext ctx = { whitespace };

      auto pos = file.begin();
      ast_proto m_ast;

      if (!proto_parser.parse(pos, file.end(), ctx, m_ast) || pos != file.end()) {
         decltype(pos) lineStart, lineEnd;
         auto error = pos;

         for (lineStart = pos; lineStart != file.begin() && *lineStart != '\n'; --lineStart);
         for (lineEnd = pos; lineEnd != file.end() && *lineEnd != '\n'; ++lineEnd);

         std::cout << "Syntax Error" << std::endl;
         std::cout << std::string(lineStart, lineEnd) << std::endl;
         std::cout << std::string((size_t)(pos - lineStart), ' ') << '^' << std::endl;
         return false;
      }

      std::fstream out;
      out.open("pbsl\\" + path + ".h", std::fstream::out);
      out << "#pragma once" << std::endl;
      out << "#include <pbsl/string_view.h>" << std::endl;
      out << std::endl;

      for (ast_enum &enum_ : m_ast.enums) {
         dumpEnum(out, enum_);
      }

      for (ast_message &msg : m_ast.messages) {
         dumpMessageDeclaration(out, msg);
      }

      out.close();

      out.open("pbsl\\" + path + ".cpp", std::fstream::out);
      out << "#include \"" << path << ".h\"" << std::endl;
      out << "#include <pbsl/parser.h>" << std::endl;
      out << std::endl;

      for (ast_message &msg : m_ast.messages) {
         dumpMessageDefinition(out, msg);
      }

      out.close();
   }

   return 0;
}
