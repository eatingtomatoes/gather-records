#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace {

struct PrettyStream {
  PrettyStream(llvm::raw_ostream &os, const std::string &padding)
      : os_(os), padding(padding), level(0), last_char(0) {}

  llvm::raw_ostream &os_;
  std::string padding;
  int level;
  char last_char;

  void indent() { ++this->level; }

  void dedent() { --this->level; }

  template <typename T>
  auto operator<<(const T &obejct) -> decltype(llvm::outs() << obejct, *this) {

    std::string text;
    {
      llvm::raw_string_ostream os(text);
      os << obejct;
      os.str();
    }

    int num_chars = text.size();

    for (int i = 0; i < num_chars; ++i) {
      if (last_char == '\n' || last_char == '\r')
        this->pad();

      os_ << text[i];
      last_char = text[i];
    }

    return *this;
  }

  PrettyStream &pad() {
    for (int i = 0; i < this->level; ++i) {
      os_ << padding;
    }

    return *this;
  }
} pretty_outs(llvm::outs(), "  |  ");

bool starts_with(const char *str, const char *prefix) {
  return std::strstr(str, prefix) == prefix;
}

struct TypeAlias {
  std::string from;
  std::string to;
};

struct ConstantDef {
  std::string name;
  std::string type;
  std::string value;
};

struct Record {
  std::string name;
  std::vector<TypeAlias> type_aliases;
  std::vector<ConstantDef> constant_defs;
};

class GatherRecordsVisitor : public RecursiveASTVisitor<GatherRecordsVisitor> {
private:
  ASTContext &context;
  std::vector<Record> records;

public:
  GatherRecordsVisitor(ASTContext &context) : context(context) {}

  bool VisitType(Type *type) {
    this->explore_type(type->getCanonicalTypeInternal());
    return true;
  }

  const std::vector<Record> &GetRecords() const { return records; }

private:
  void explore_type(QualType type) {
    std::string canonical_type_name = type.getAsString();

    if (starts_with(canonical_type_name.c_str(), "_") ||
        starts_with(canonical_type_name.c_str(), "__"))
      return;

    if (auto temp_spec_type = dyn_cast<TemplateSpecializationType>(type)) {
      if (temp_spec_type->isSugared())
        explore_type(temp_spec_type->desugar());

      return;
    }

    if (auto record_type = dyn_cast<RecordType>(type)) {
      Record record;
      record.name = canonical_type_name;

      if (extract_record(record_type->getDecl(), &record))
        this->records.push_back(record);

      return;
    }
  }

  bool extract_record(RecordDecl *record_decl, Record *record) const {
    int num_effective_fields = 0;

    for (Decl *sub_decl : record_decl->decls()) {
      if (auto type_alias_decl = dyn_cast<TypeAliasDecl>(sub_decl)) {
        TypeAlias type_alias;

        if (extrat_type_alias(type_alias_decl, &type_alias)) {
          record->type_aliases.push_back(type_alias);
          ++num_effective_fields;
        }

        continue;
      }

      if (auto var_decl = dyn_cast<VarDecl>(sub_decl)) {
        ConstantDef constant_def;

        if (extract_contant_def(var_decl, &constant_def)) {
          record->constant_defs.push_back(constant_def);
          ++num_effective_fields;
        }

        continue;
      }
    }

    return num_effective_fields > 0;
  }

  bool extrat_type_alias(const TypeAliasDecl *type_alias_decl,
                         TypeAlias *type_alias) const {
    type_alias->to = type_alias_decl->getName();

    QualType underlying_type = type_alias_decl->getUnderlyingType();

    if (auto subst_type =
            dyn_cast<SubstTemplateTypeParmType>(underlying_type)) {
      type_alias->from = subst_type->getReplacementType()
                             .getDesugaredType(context)
                             .getAsString();
    } else {
      type_alias->from =
          underlying_type.getDesugaredType(context).getAsString();
    }

    return true;
  }

  bool extract_contant_def(VarDecl *var_decl, ConstantDef *constant_def) const {
    if (!var_decl->isConstexpr())
      return false;

    constant_def->name = var_decl->getName();
    constant_def->type = var_decl->getType().getAsString();

    do {
      if (EvaluatedStmt *stmt = var_decl->ensureEvaluatedStmt()) {
        if (stmt->WasEvaluated) {
          constant_def->value =
              stmt->Evaluated.getAsString(context, var_decl->getType());
          break;
        }
      }

      constant_def->value = "<unresolved>";
    } while (false);

    return true;
  }
};

void dump_records_as_json(const std::vector<Record> &records) {

  PrettyStream os(llvm::outs(), "    ");

  os << "[\n";

  os.indent();

  const char *record_delimiter = "";

  for (const Record &rec : records) {
    os << std::exchange(record_delimiter, ",") << "{\n";
    os.indent();

    os << "\"record_name\": \"" << rec.name << "\",\n";

    {
      os << "\"type_aliases\": [\n";
      os.indent();

      const char *type_alias_delimiter = "";

      for (const TypeAlias &type_alias : rec.type_aliases) {
        os << std::exchange(type_alias_delimiter, ",") << "{\n";
        os.indent();

        os << "\"from\": \"" << type_alias.from << "\",\n";
        os << "\"to\": \"" << type_alias.to << "\"\n";

        os.dedent();
        os << "}\n";
      }

      os.dedent();
      os << "],\n";
    }

    {
      os << "\"constants\": [\n";
      os.indent();

      const char *constant_delimiter = "";

      for (const ConstantDef &constant_def : rec.constant_defs) {
        os << std::exchange(constant_delimiter, ",") << "{\n";
        os.indent();

        os << "\"name\": \"" << constant_def.name << "\",\n";
        os << "\"type\": \"" << constant_def.type << "\",\n";
        os << "\"value\": \"" << constant_def.value << "\"\n";

        os.dedent();
        os << "}\n";
      }

      os.dedent();
      os << "]\n";
    }

    os.dedent();
    os << "}\n";
  }

  os.dedent();
  os << "]";
}

class GatherRecordsConsumer : public clang::ASTConsumer {
public:
  virtual void HandleTranslationUnit(clang::ASTContext &Context) override {
    GatherRecordsVisitor visitor(Context);

    visitor.TraverseDecl(Context.getTranslationUnitDecl());
    dump_records_as_json(visitor.GetRecords());
  }
};

class GatherRecordsAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<GatherRecordsConsumer>();
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<GatherRecordsAction>
    X("gather-records", "gather various records' defintions");
