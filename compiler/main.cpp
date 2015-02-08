#include "parser.h"
#include <filesystem>
#include <iostream>
#include <cassert>
#include <algorithm>

size_t IndentSize = 3;

std::map<std::string, Type> DeclarationTypeMap;

void addIndent(std::string &indent)
{
   indent.append(IndentSize, ' ');
}

void subIndent(std::string &indent)
{
   indent.resize(indent.size() - IndentSize);
}

std::string getWireTypeName(TypeInfo &type)
{
   switch (type.basicType) {
   case Type::Bool:
   case Type::Int32:
   case Type::Int64:
   case Type::Uint32:
   case Type::Uint64:
   case Type::Sint32:
   case Type::Sint64:
   case Type::Enum:
      return "VarInt";
   case Type::Float:
   case Type::Fixed32:
   case Type::Sfixed32:
      return "Fixed32";
   case Type::Double:
   case Type::Fixed64:
   case Type::Sfixed64:
      return "Fixed64";
   case Type::String:
   case Type::Bytes:
   case Type::Message:
   case Type::MessagePointer:
      return "LengthDelimited";
   default:
      assert(false);
      return "ERROR";
   }
}

void dumpEnum(std::ostream &out, Enum &enum_, std::string indent)
{
   out << indent << "enum " << enum_.name << " {" << std::endl;
   addIndent(indent);

   for (auto &field : enum_.fields) {
      out << indent << field.name << " = " << field.value << "," << std::endl;
   }

   subIndent(indent);
   out << indent << "};" << std::endl;
}

void dumpMessageDeclaration(std::ostream &out, Message &msg, std::string indent)
{
   // Dump message struct
   out << indent << "struct " << msg.name << std::endl;
   out << indent << "{" << std::endl;
   addIndent(indent);

   // Dump child enums
   for (Enum &enum_ : msg.enums) {
      dumpEnum(out, enum_, indent);
      out << std::endl;
   }

   // Dump child messages
   for (Message &submsg : msg.messages) {
      dumpMessageDeclaration(out, submsg, indent);
      out << std::endl;
   }

   // Dump fields
   for (Field &field : msg.fields) {
      if (field.type.basicType == Type::MessagePointer) {
         if (field.rule == FieldRule::Repeated) {
            out << indent << "std::vector<std::unique_ptr<" << field.nativeType << ">> " << field.nativeName << ";" << std::endl;
         } else {
            out << indent << "std::unique_ptr<" << field.nativeType << "> " << field.nativeName << ";" << std::endl;
         }
      } else {
         if (field.rule == FieldRule::Repeated) {
            out << indent << "std::vector<" << field.nativeType << "> " << field.nativeName << ";" << std::endl;
         } else {
            out << indent << field.nativeType << " " << field.nativeName << ";" << std::endl;
         }
      }
   }

   out << indent << "bool parse(const std::string_view &data);" << std::endl;
   subIndent(indent);
   out << indent << "};" << std::endl;
}

void dumpHeaderFile(ProtoFile &proto)
{
   std::ofstream out("pbsl/" + proto.name + ".pbsl.h");
   out << "#pragma once" << std::endl;
   out << "#include <pbsl/declaration.h>" << std::endl;

   // Dump imports as #include
   for (Import &import : proto.imports) {
      if (import.file.find("google") != std::string::npos) {
         continue;
      }

      auto file = std::tr2::sys::path(import.file).basename();
      out << "#include \"" << file + ".pbsl.h\"" << std::endl;
   }

   out << std::endl;

   // Dump enums
   for (Enum &enum_ : proto.enums) {
      dumpEnum(out, enum_, "");
      out << std::endl;
   }

   // Dump messages
   for (Message &msg : proto.messages) {
      dumpMessageDeclaration(out, msg, "");
      out << std::endl;
   }

   out.close();
}

void dumpMessageParser(std::ostream &out, Message &msg, std::string indent)
{
   static const std::map<Type, std::string> ReadTypeMap = {
      { Type::Double, "readDouble" },
      { Type::Float, "readFloat" },
      { Type::Int32, "readInt32" },
      { Type::Int64, "readInt64" },
      { Type::Uint32, "readUint32" },
      { Type::Uint64, "readUint64" },
      { Type::Sint32, "readSint32" },
      { Type::Sint64, "readSint64" },
      { Type::Fixed32, "readFixed32" },
      { Type::Fixed64, "readFixed64" },
      { Type::Sfixed32, "readSfixed32" },
      { Type::Sfixed64, "readSfixed64" },
      { Type::Bool, "readBool" },
      { Type::String, "readString" },
      { Type::Bytes, "readBytes" }
   };

   for (Message &submsg : msg.messages) {
      dumpMessageParser(out, submsg, "");
      out << std::endl;
   }

   out << indent << "bool " << msg.nativeName << "::parse(const std::string_view &data__)" << std::endl;
   out << indent << "{" << std::endl;
   addIndent(indent);
   if (msg.fields.size() == 0) {
      out << indent << "return true;" << std::endl;
   } else {
      out << indent << "auto parser__ = pbsl::Parser { data__ };" << std::endl;
      out << std::endl;

      out << indent << "while(!parser__.eof()) {" << std::endl;
      addIndent(indent);
      {
         out << indent << "auto tag__ = parser__.readTag();" << std::endl;
         out << std::endl;
         out << indent << "switch(tag__.field) {" << std::endl;

         for (auto &field : msg.fields) {
            out << indent << "case " << field.value << ":" << std::endl;
            addIndent(indent);
            out << indent << "assert(tag__.type == pbsl::Parser::WireType::" << getWireTypeName(field.type) << ");" << std::endl;

            auto readItr = ReadTypeMap.find(field.type.basicType);
            if (readItr == ReadTypeMap.end()) {
               if (field.type.basicType == Type::Message) {
                  if (field.rule == FieldRule::Repeated) {
                     out << indent << field.nativeName << ".emplace_back();" << std::endl;
                     out << indent << field.nativeName << ".back().parse(parser__.readString());" << std::endl;
                  } else {
                     out << indent << field.nativeName << ".parse(parser__.readString());" << std::endl;
                  }
               } else if (field.type.basicType == Type::MessagePointer) {
                  if (field.rule == FieldRule::Repeated) {
                     out << indent << field.nativeName << ".emplace_back(new " << field.nativeAbsoluteType << "()); " << std::endl;
                     out << indent << field.nativeName << ".back()->parse(parser__.readString());" << std::endl;
                  } else {
                     out << indent << field.nativeName << " = std::make_unique<" << field.nativeAbsoluteType << ">();" << std::endl;
                     out << indent << field.nativeName << "->parse(parser__.readString());" << std::endl;
                  }
               } else if (field.type.basicType == Type::Enum) {
                  if (field.rule == FieldRule::Repeated) {
                     out << indent << field.nativeName << ".push_back(static_cast<" << field.nativeType << ">(parser__.readUint32()));" << std::endl;
                  } else {
                     out << indent << field.nativeName << " = static_cast<" << field.nativeType << ">(parser__.readUint32());" << std::endl;
                  }
               } else {
                  assert(false);
               }
            } else {
               if (field.rule == FieldRule::Repeated) {
                  out << indent << field.nativeName << ".push_back(parser__." << readItr->second << "());" << std::endl;
               } else {
                  out << indent << field.nativeName << " = parser__." << readItr->second << "();" << std::endl;
               }
            }

            out << indent << "break;" << std::endl;
            subIndent(indent);
         }

         out << indent << "default:" << std::endl;
         addIndent(indent);
         {
            out << indent << "if (!parser__.eof()) {" << std::endl;
            addIndent(indent);
            {
               out << indent << "assert(0 && \"Invalid field number!\");" << std::endl;
            }
            subIndent(indent);
            out << indent << "}" << std::endl;
            out << indent << "return false;" << std::endl;
         }
         subIndent(indent);
         out << indent << "}" << std::endl;
      }
      subIndent(indent);
      out << indent << "}" << std::endl;

      out << std::endl;
      out << indent << "return true;" << std::endl;
   }
   subIndent(indent);
   out << indent << "};" << std::endl;
}

void dumpSourceFile(ProtoFile &proto)
{
   if (proto.messages.size() == 0) {
      return;
   }

   std::ofstream out("pbsl/" + proto.name + ".pbsl.cpp");
   out << "#include \"" + proto.name + ".pbsl.h\"" << std::endl;
   out << "#include <pbsl/parser.h>" << std::endl;
   out << std::endl;

   // Dump messages
   for (Message &msg : proto.messages) {
      dumpMessageParser(out, msg, "");
      out << std::endl;
   }

   out.close();
}

std::string convertClassName(const std::string &in)
{
   std::string nativeClass = in;
   std::string::size_type pos;

   if (nativeClass.at(0) == '.') {
      nativeClass.erase(0, 1);
   }

   while ((pos = nativeClass.find_first_of('.')) != std::string::npos) {
      nativeClass.replace(pos, 1, "::", 2);
   }

   return nativeClass;
}

static std::vector<std::string> RestrictedWords = {
   "template"
};

static const std::map<Type, std::string> NativeTypeMap = {
   { Type::Double, "double" },
   { Type::Float, "float" },
   { Type::Int32, "int32_t" },
   { Type::Int64, "int64_t" },
   { Type::Uint32, "uint32_t" },
   { Type::Uint64, "uint64_t" },
   { Type::Sint32, "int32_t" },
   { Type::Sint64, "int64_t" },
   { Type::Fixed32, "uint32_t" },
   { Type::Fixed64, "uint64_t" },
   { Type::Sfixed32, "int32_t" },
   { Type::Sfixed64, "int64_t" },
   { Type::Bool, "bool" },
   { Type::String, "std::string_view" },
   { Type::Bytes, "std::string_view" }
};

void createNativeNames(Enum &enum_, std::string path = "")
{
   enum_.nativeName = path + enum_.name;
   DeclarationTypeMap[enum_.nativeName] = Type::Enum;
   path += enum_.name + "::";

   for (auto &field : enum_.fields) {
      if (std::find(RestrictedWords.begin(), RestrictedWords.end(), field.name) != RestrictedWords.end()) {
         field.nativeName = field.name + "_";
      } else {
         field.nativeName = field.name;
      }
   }
}

void createNativeNames(Message &msg, std::string path = "")
{
   msg.nativeName = path + msg.name;
   DeclarationTypeMap[msg.nativeName] = Type::Message;
   path += msg.name + "::";

   for (auto &field : msg.fields) {
      if (std::find(RestrictedWords.begin(), RestrictedWords.end(), field.name) != RestrictedWords.end()) {
         field.nativeName = field.name + "_";
      } else {
         field.nativeName = field.name;
      }

      auto typeItr = NativeTypeMap.find(field.type.basicType);

      if (typeItr != NativeTypeMap.end()) {
         field.nativeType = typeItr->second;
      } else {
         field.nativeAbsoluteType = convertClassName(field.type.className);
         field.nativeType = field.nativeAbsoluteType;

         if (field.nativeType.find(path) == 0) {
            field.nativeType.erase(0, path.size());
         }
      }
   }

   for (auto &enum_ : msg.enums) {
      createNativeNames(enum_, path);
   }

   for (auto &msg : msg.messages) {
      createNativeNames(msg, path);
   }
}

void resolveLookupTypes(Message &msg)
{
   for (auto &field : msg.fields) {
      if (field.type.basicType == Type::LookupName) {
         if (field.nativeAbsoluteType.compare(msg.nativeName) == 0) {
            field.type.basicType = Type::MessagePointer;
         } else {
            auto itr = DeclarationTypeMap.find(field.nativeAbsoluteType);

            if (itr == DeclarationTypeMap.end()) {
               // TODO: It must be from the import - awh fuk?!
               field.type.basicType = Type::Message;
            } else {
               field.type.basicType = itr->second;
            }
         }
      }
   }

   for (auto &msg : msg.messages) {
      resolveLookupTypes(msg);
   }
}

void reorderMessageDependences(std::vector<Message> &messages)
{
   for (auto itr = messages.begin(); itr != messages.end(); ++itr) {
      auto &msg = *itr;

      for (auto &field : msg.fields) {
         auto find = field.nativeAbsoluteType;
         auto found = std::find_if(messages.begin(), messages.end(), [&find](Message &search){
               return search.name.compare(find) == 0;
            });

         if (found != messages.end()) {
            if (found > itr) {
               std::iter_swap(found, itr);
               break;
            }
         }
      }
   }
}

int main(int argc, char **argv)
{
   std::map<std::string, ProtoFile> protos;

   for (auto i = 1; i < argc; ++i) {
      std::tr2::sys::path path(argv[i]);
      auto filename = path.leaf();
      auto proto = protos[filename];
      proto.name = path.basename();

      if (!parseFile(path, proto)) {
         std::cout << "Parse failed for file " << path << std::endl;
         return -1;
      }

      // Resolve all the .nativeName and .nativeType properties
      for (auto &enum_ : proto.enums) {
         createNativeNames(enum_);
      }

      for (auto &msg : proto.messages) {
         createNativeNames(msg);
      }

      // Resolve all Type::LookupNames so we know whether to parse an enum or msg
      for (auto &msg : proto.messages) {
         resolveLookupTypes(msg);
      }

      // Reorder dependencies
      reorderMessageDependences(proto.messages);
      
      // Dump our header file!
      dumpHeaderFile(proto);

      // Dump our source file!
      dumpSourceFile(proto);
   }

   return 0;
}
