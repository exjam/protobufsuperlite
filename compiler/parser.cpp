#include "parser.h"
#include <fstream>
#include <iostream>
#include <prs/parser.h>

using namespace prs;

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
parse_rule<ast_stringify> string_rule;
auto string_def =
   atomic(char_('"') >> +(!char_('"') >> char_any()) >> char_('"'));

// float number -0.5
parse_rule<ast_stringify> number_rule;
auto number_def =
   atomic(-char_('-') >> +digit >> -(char_('.') >> +digit));

// symbol / identifier
parse_rule<ast_stringify> symbol_rule;
auto symbol_def =
   atomic(+(letter | digit | char_('_') | char_('.') | char_('(') | char_(')')));

// field_value
auto field_value =
   (number_rule | symbol_rule | string_rule);

// TODO: Support multiple options
// field_option
parse_rule<ast_field_options> field_options_rule;
auto field_option_value = ast<FieldOption>() >>
   (symbol_rule >> char_('=') >> field_value);

auto field_options_def =
   char_('[') >>
   field_option_value >>
   char_(']');

// property
parse_rule<Field> property_rule;
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
parse_rule<Option> option_rule;
auto option_def =
   string_("option") >>
   symbol_rule >> // name
   char_('=') >>
   field_value >> // value
   char_(';');

// enum
parse_rule<Enum> enum_rule;
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
parse_rule<Import> import_rule;
auto import_def =
   string_("import") >>
   string_rule >> // "path"
   char_(';');

// message
parse_rule<Message> message_rule;
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
parse_rule<Extend> extend_rule;
auto extend_def =
   string_("extend") >>
   symbol_rule >> // name
   char_('{') >>
   *(property_rule) >> // properties
   char_('}');

// .proto parser
parse_rule<ProtoFile> proto_parser;
auto proto_def =
   *(enum_rule
   | option_rule
   | message_rule
   | import_rule
   | extend_rule);

bool initParser()
{
   static bool isParserStarted = false;

   if (!isParserStarted) {
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
      isParserStarted = true;
   }

   return true;
}

bool parseFile(std::string path, ProtoFile &result)
{
   auto file = std::fstream { path, std::fstream::in };
   auto data = std::string {};

   if (!file.is_open()) {
      std::cout << "Could not open " << path << " for reading" << std::endl;
      return false;
   }

   // Read in whole file
   file.seekg(0, file.end);
   auto size = static_cast<std::size_t>(file.tellg());
   file.seekg(0, file.beg);
   data.resize(size);
   file.read(&data[0], size);
   file.close();

   // Do parse!
   ParseContext ctx = { whitespace };
   auto pos = data.begin();
   initParser();

   if ((!proto_parser.parse(pos, data.end(), ctx, result) || pos != data.end()) && *pos != 0) {
      decltype(pos) lineStart, lineEnd;
      auto error = pos;

      for (lineStart = pos; lineStart != data.begin() && *lineStart != '\n'; --lineStart);
      for (lineEnd = pos; lineEnd != data.end() && *lineEnd != '\n'; ++lineEnd);

      std::cout << "Syntax Error" << std::endl;
      std::cout << std::string(lineStart, lineEnd) << std::endl;
      std::cout << std::string(static_cast<size_t>(pos - lineStart), ' ') << '^' << std::endl;
      return false;
   }

   return true;
}
