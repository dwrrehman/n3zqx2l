///n: a n3zqx2l compiler written in C++.
#include "llvm/AsmParser/Parser.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSummaryIndex.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/SourceMgr.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#define prep(_level)            for (nat i = _level; i--;) std::cout << ".   "

using nat = int_fast64_t;
using llvm_module = std::unique_ptr<llvm::Module>;
using llvm_modules = std::vector<llvm_module>;

struct expression;   struct symbol;    struct symbol_table;

enum class lex_type {none, string, identifier, llvm_string};
enum class output_type {none, llvm, assembly, object_file, executable};
enum class token_type {null, string, identifier, llvm, operator_};
enum class symbol_type { none, subexpression, string_literal, llvm_literal, identifier };
namespace intrinsic { enum intrinsic_index { typeless, type, infered, llvm, none, application, abstraction, define, evaluate }; }

struct file {
    const char* name = "";
    std::string text = "";
    bool contains_main = false;
};

struct arguments {
    std::vector<file> files = {};
    enum output_type output = output_type::none;
    const char* name = "";
    bool includes_standard_library = true;
};

struct token {
    token_type type = token_type::null;
    std::string value = "";
    nat line = 0;
    nat column = 0;
};

struct lexing_state {
    nat c = 0;
    nat line = 0;
    nat column = 0;
    lex_type state = lex_type::none;
    token current = {};
    const char* filename = "";
    std::string text = "";
};

struct string_literal { token literal = {}; bool error = 0; };
struct llvm_literal { token literal = {}; bool error = 0; };
struct identifier { token name = {}; bool error = 0; };

struct expression {
    std::vector<symbol> symbols = {};
    nat type = 0;
    struct token start = {};
    bool error = false;
};

struct symbol {
    symbol_type type = symbol_type::none;
    expression subexpression = {};
    string_literal string = {};
    llvm_literal llvm = {};
    identifier identifier = {};
    bool error = false;
};

struct program_data {
    file file;
    llvm::Module* module;
    llvm::IRBuilder<>& builder;
};

struct resolve_state { symbol_table& stack; program_data& data; };

struct resolved_expression {
    nat index = 0;
    std::vector<resolved_expression> args = {};
    llvm::Type* llvm_type = nullptr;
    expression signature = {};
    llvm::Value* constant = {};
    bool error = false;
};

struct signature_entry {
    expression signature = {};
    expression definition = {};
    nat precedence = 0;
    llvm::Value* value = nullptr;
    llvm::Function* function = nullptr;
    llvm::Type* llvm_type = nullptr;
};

struct symbol_table {
    std::vector<signature_entry> master = {};
    std::vector<std::vector<nat>> frames = {};
    struct program_data& data;
    
    symbol_table(program_data& data, const std::vector<expression>& builtins);
    void update(llvm::ValueSymbolTable& llvm);
    void push_new_frame();
    void pop_last_frame();
    std::vector<nat>& top();
    expression& get(nat index);
    void define(const expression& signature, const expression& definition, nat back_from, nat parent = 0);
};

symbol id(std::string name) { return {symbol_type::identifier, {}, {}, {}, {{token_type::identifier, name}}}; }
symbol param(nat type) { return {symbol_type::subexpression, {{}, type}}; }
// constants:
static expression failure = {{}, 0, {}, true};
static const resolved_expression resolution_failure = {0, {}, nullptr, {}, {}, false};

static expression infered_type = {{id("__")}, intrinsic::typeless};
static expression type_type = {{id("_")}, intrinsic::typeless};
static expression llvm_type = {{id("_llvm")}, intrinsic::typeless};

static expression none_type = {{id("_0")}, intrinsic::type};
static expression application_type = {{id("_a")}, intrinsic::type};
static expression abstraction_type = {{id("_b")}, intrinsic::type};

static expression confer_abstraction = {{id("_c"), param(intrinsic::llvm)}, intrinsic::llvm};
static expression define_abstraction = {{id("_d"), param(intrinsic::abstraction), param(intrinsic::type), param(intrinsic::application), param(intrinsic::application) }, intrinsic::llvm}; // returns unit.
static expression evaluate_abstraction = {{id("_e"), param(intrinsic::application)}, intrinsic::llvm};  // returns unit.

static expression chain = {{param(intrinsic::type), param(intrinsic::type)}, intrinsic::type};
static expression hello_test = {{id("hello")}, intrinsic::type};

static const std::vector<expression> builtins = {
    type_type, infered_type, llvm_type, none_type,
    application_type, abstraction_type,
    define_abstraction, evaluate_abstraction,
    
    hello_test, chain
};

static nat max_expression_depth = 5;
static bool debug = false;
lexing_state l = {}; ///TODO: eliminate this global.

void print_expression(expression e, nat d);
expression parse(const file& file);
resolved_expression resolve_expression(const expression& given, nat given_type, llvm::Function*& function, resolve_state& state);
resolved_expression resolve(const expression& given, nat given_type, llvm::Function*& function, nat& index, nat depth, nat max_depth, nat fdi_length, resolve_state& state);

static inline arguments get_arguments(const int argc, const char** argv) {
    arguments args = {};
    for (nat i = 1; i < argc; i++) {
        const auto word = std::string(argv[i]);
        if (word == "-z") debug = true;
        else if (word == "-u") { printf("usage: n -[zuverscod/-] <.n/.ll/.o/.s>\n"); exit(0); }
        else if (word == "-v") { printf("n3zqx2l: 0.0.3 \tn: 0.0.3\n"); exit(0); }
        else if (word == "-e") args.includes_standard_library = false;
        else if (word == "-r" and i + 1 < argc) { args.output = output_type::llvm; args.name = argv[++i]; }
        else if (word == "-s" and i + 1 < argc) { args.output = output_type::assembly; args.name = argv[++i]; }
        else if (word == "-c" and i + 1 < argc) { args.output = output_type::object_file; args.name = argv[++i]; }
        else if (word == "-o" and i + 1 < argc) { args.output = output_type::executable; args.name = argv[++i]; }
        else if (word == "-d" and i + 1 < argc) { auto n = atoi(argv[++i]); max_expression_depth = n ? n : 4; }
        else if (word == "-/") { break; /*the linker argumnets start here.*/ }
        else if (word == "--") { break; /*the interpreter argumnets start here.*/ }
        else if (word[0] == '-') { printf("n: error: bad option: %s\n", argv[i]); exit(1); }
        else {
            std::ifstream stream {argv[i]};
            if (stream.good()) {
                std::string text {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
                stream.close();
                args.files.push_back({argv[i], text});
            } else { printf("n: error: unable to open \"%s\": %s\n", argv[i], strerror(errno)); exit(1); }
        }
    }
    if (args.files.empty()) { printf("n: error: no input files\n"); exit(1); }
    return args;
}

static inline void set_current(token_type t, enum lex_type s) { l.current = {t, "", l.line, l.column}; l.state = s; }
static inline void advance_by(nat n) { for (nat i = n; i--;) if (l.text[l.c++] == '\n') { l.line++; l.column = 1; } else l.column++; }
static inline bool is_identifier(char c) { return isalnum(c) or c == '_'; }
static inline bool is_operator(char c) { return (not is_identifier(c) or not isascii(c)) and c != ' ' and c != '\t' and c != '\n'; }
static inline bool is_valid(nat c) { return c >= 0 and c < (nat) l.text.size(); }
static inline bool is_operator(const token& t) { return t.type == token_type::operator_; }
static inline bool is_identifier(const token& t) { return t.type == token_type::identifier; }
static inline bool is_close_paren(const token& t) { return is_operator(t) and t.value == ")"; }
static inline bool is_open_paren(const token& t) { return is_operator(t) and t.value == "("; }
static inline bool is_reserved_operator(token t) { return is_open_paren(t) or is_close_paren(t); }
static inline bool subexpression(const symbol& s) { return s.type == symbol_type::subexpression; }
static inline bool identifier(const symbol& s) { return s.type == symbol_type::identifier; }
static inline bool llvm_string(const symbol& s) { return s.type == symbol_type::llvm_literal; }
static inline bool parameter(const symbol &symbol) { return subexpression(symbol); }
static inline bool are_equal_identifiers(const symbol &first, const symbol &second) { return identifier(first) and identifier(second) and first.identifier.name.value == second.identifier.name.value; }
static inline const char* stringify_token(enum token_type type) { switch (type) { case token_type::null: return "EOF"; case token_type::string: return "string"; case token_type::identifier: return "identifier"; case token_type::operator_: return "operator";case token_type::llvm: return "llvm"; } }

static inline void check_for_lexing_errors() {
    if (l.state == lex_type::string) printf("n3zqx2l: %s:%lld,%lld: error: unterminated string\n", l.filename, l.line, l.column);
    else if (l.state == lex_type::llvm_string) printf("n3zqx2l: %s:%lld,%lld: error: unterminated llvm string\n", l.filename, l.line, l.column);
    if (l.state == lex_type::string or l.state == lex_type::llvm_string) exit(1);
}

static inline token next() {
    while (true) {
        if (l.c >= (nat) l.text.size()) { check_for_lexing_errors(); return {token_type::null, "", l.line, l.column}; }
        else if (is_identifier(l.text[l.c]) and is_valid(l.c+1) and not is_identifier(l.text[l.c+1]) and l.state == lex_type::none) {
            set_current(token_type::identifier, lex_type::none);
            l.current.value = l.text[l.c];
            advance_by(1);
            auto result = l.current; l.current = {}; return result;
        } else if (l.text[l.c] == '\"' and l.state == lex_type::none) { set_current(token_type::string, lex_type::string); }
        else if (l.text[l.c] == '`' and l.state == lex_type::none) { set_current(token_type::llvm, lex_type::llvm_string); }
        else if (is_identifier(l.text[l.c]) and (l.state == lex_type::none)) {
            set_current(token_type::identifier, lex_type::identifier);
            l.current.value += l.text[l.c];
        } else if (l.text[l.c] == '\\' and l.state == lex_type::string) {
            if (is_valid(l.c+1) and l.text[l.c+1] == '\\') { l.current.value += "\\"; advance_by(1); }
            else if (is_valid(l.c+1) and l.text[l.c+1] == '\"') { l.current.value += "\""; advance_by(1); }
            else if (is_valid(l.c+1) and l.text[l.c+1] == 'n') { l.current.value += "\n"; advance_by(1); }
            else if (is_valid(l.c+1) and l.text[l.c+1] == 't') { l.current.value += "\t"; advance_by(1); }
        } else if ((l.text[l.c] == '\"' and l.state == lex_type::string)  or (l.text[l.c] == '`' and l.state == lex_type::llvm_string)) {
            l.state = lex_type::none;
            advance_by(1);
            auto result = l.current; l.current = {}; return result;
        } else if (is_identifier(l.text[l.c]) and is_valid(l.c+1) and not is_identifier(l.text[l.c+1]) and l.state == lex_type::identifier) {
            l.current.value += l.text[l.c];
            l.state = lex_type::none;
            advance_by(1);
            auto result = l.current; l.current = {}; return result;
        } else if (l.state == lex_type::string or l.state == lex_type::llvm_string or (is_identifier(l.text[l.c]) and l.state == lex_type::identifier)) l.current.value += l.text[l.c];
        else if (is_operator(l.text[l.c]) and l.state == lex_type::none) {
            set_current(token_type::operator_, lex_type::none);
            l.current.value = l.text[l.c];
            advance_by(1);
            auto result = l.current; l.current = {}; return result;
        } advance_by(1);
    }
}

///TODO: redo the lexer so we done need these functions:
static inline lexing_state save() { return l; }
static inline void revert(lexing_state s) { l = s; }
static inline file start_lex(const file& file) { l = {0, 1, 1, lex_type::none, {}, file.name, file.text}; return file; }

static inline struct string_literal parse_string_literal() {
    auto saved = save();
    auto t = next();
    if (t.type != token_type::string) { revert(saved); return {{}, true}; }
    return {t};
}

static inline llvm_literal parse_llvm_literal() {
    auto saved = save();
    auto t = next();
    if (t.type != token_type::llvm) { revert(saved); return {{}, true}; }
    return {t};
}

static inline struct identifier parse_identifier() {
    auto saved = save();
    auto t = next();
    if (not is_identifier(t) and (is_reserved_operator(t) or not is_operator(t))) { revert(saved); return {{}, true}; }
    return {t};
}

static inline symbol parse_symbol(const file& file) {
    auto saved = save(); auto t = next();
    if (is_open_paren(t)) {
        auto e = parse(file);
        if (not e.error) {
            auto saved_t = t;
            t = next();
            e.start = t;
            if (is_close_paren(t)) return {symbol_type::subexpression, e};
            else {
                printf("n3zqx2l: %s:%lld:%lld: unexpected %s, \"%s\", expected \")\" to close expression\n", file.name, saved_t.line, saved_t.column, stringify_token(t.type), t.value.c_str());
                revert(saved); return {symbol_type::none, {}, {}, {}, {}, true};
            }
        } else revert(saved);
    } else revert(saved);
    if (auto string = parse_string_literal(); not string.error) return {symbol_type::string_literal, {}, string}; else revert(saved);
    if (auto llvm = parse_llvm_literal(); not llvm.error) return {symbol_type::llvm_literal, {}, {}, llvm}; else revert(saved);
    if (auto identifier = parse_identifier(); not identifier.error) return {symbol_type::identifier, {}, {}, {}, identifier}; else revert(saved);
    return {symbol_type::none, {}, {}, {}, {}, true};
}

expression parse(const file& file) {
    std::vector<symbol> symbols = {};
    auto saved = save(); auto start = next(); revert(saved);
    auto symbol = parse_symbol(file);
    while (not symbol.error) {
        symbols.push_back(symbol);
        saved = save();
        symbol = parse_symbol(file);
    }
    revert(saved);
    expression result = {symbols};
    result.start = start;
    return result;
}

static inline std::string expression_to_string(const expression& given, symbol_table& stack) {
    std::string result = "(";
    nat i = 0;
    for (auto symbol : given.symbols) {
        if (symbol.type == symbol_type::identifier) result += symbol.identifier.name.value;
        else if (symbol.type == symbol_type::subexpression) result += "(" + expression_to_string(symbol.subexpression, stack) + ")";
        if (i++ < (nat) given.symbols.size() - 1) result += " ";
    }
    result += ")";
    if (given.type) result += " " + expression_to_string(stack.master[given.type].signature, stack);
    return result;
}

static inline void print_resolved_expr(resolved_expression expr, nat depth, resolve_state& state) {
    prep(depth); std::cout << "[error = " << std::boolalpha << expr.error << "]\n";
    prep(depth); std::cout << "index = " << expr.index << " :: " << expression_to_string(state.stack.get(expr.index), state.stack);
    if (expr.signature.symbols.size()) std::cout << " ::: " << expression_to_string(expr.signature, state.stack);
    std::cout << "\n";
    if (expr.llvm_type) { prep(depth); std::cout << "llvm type = "; expr.llvm_type->print(llvm::errs()); }
    std::cout << "\n";
    nat i = 0;
    for (auto arg : expr.args) {
        prep(depth + 1); std::cout << "argument #" << i++ << ": \n";
        print_resolved_expr(arg, depth + 2, state);
        prep(depth); std::cout << "\n";
    }
}

static inline void debug_table(symbol_table table) {
    std::cout << "---- debugging stack: ----\n";
    std::cout << "printing frames: \n";
    for (auto i = 0; i < (nat) table.frames.size(); i++) {
        std::cout << "\t ----- FRAME # "<<i<<"---- \n\t\tidxs: { ";
        for (auto index : table.frames[i]) std::cout << index << " ";
        std::cout << "}\n";
    }
    std::cout << "\nmaster: {\n";
    auto j = 0;
    for (auto entry : table.master) {
        std::cout << "\t" << std::setw(6) << j << ": ";
        std::cout << expression_to_string(entry.signature, table) << "\n";
        if (entry.value) {
            std::cout << "\tLLVM value: \n";
            entry.value->print(llvm::errs());
        }
        if (entry.function) {
            std::cout << "\tLLVM function: \n";
            entry.function->print(llvm::errs());
        } j++;
    }
    std::cout << "}\n";
}

static inline llvm::Function* create_main(llvm::IRBuilder<>& builder, llvm::LLVMContext& context, llvm_module& module) {
    std::vector<llvm::Type*> state = {llvm::Type::getInt32Ty(context), llvm::Type::getInt8PtrTy(context)->getPointerTo()};
    auto main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), state, false);
    auto main_function = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", main_function));
    return main_function;
}

static inline void prune_extraneous_subexpressions(expression& given) {
    while (given.symbols.size() == 1 and subexpression(given.symbols.front())) given.symbols = given.symbols.front().subexpression.symbols;
    for (auto& symbol : given.symbols) if (subexpression(symbol)) prune_extraneous_subexpressions(symbol.subexpression);
}

static inline resolved_expression parse_llvm_type_string(llvm::Function*& function, const token& llvm_string, nat& pointer, resolve_state& state) {
    llvm::SMDiagnostic type_errors;
    if (auto llvm_type = llvm::parseType(llvm_string.value, type_errors, *state.data.module)) {
        pointer++; return {intrinsic::llvm, {}, llvm_type};
    } else {
        type_errors.print((std::string("llvm: (") + std::to_string(llvm_string.line) + "," + std::to_string(llvm_string.column) + ")").c_str(), llvm::errs());
        return {0, {}, nullptr, {}, {}, true};
    }
}

static inline resolved_expression parse_llvm_string(llvm::Function*& function, const token& llvm_string, nat& pointer, resolve_state& state) {
    llvm::SMDiagnostic function_errors; llvm::ModuleSummaryIndex my_index(true);
    llvm::MemoryBufferRef reference(llvm_string.value, state.data.file.name);
    if (not llvm::parseAssemblyInto(reference, state.data.module, &my_index, function_errors)) {
        pointer++; return {intrinsic::llvm, {}, llvm::Type::getVoidTy(state.data.module->getContext())};
    } else {
        function_errors.print((std::string("llvm: (") + std::to_string(llvm_string.line) + "," + std::to_string(llvm_string.column) + ")").c_str(), llvm::errs());
        return {0, {}, nullptr, {}, {}, true};
    }
}

static inline bool matches(const expression& given, const expression& signature, nat given_type, std::vector<resolved_expression>& args, llvm::Function*& function,
                           nat& index, nat depth, nat max_depth, nat fdi_length, resolve_state& state) {
    if (given_type != signature.type) return false;
    for (auto symbol : signature.symbols) {
        if (index >= (nat) given.symbols.size()) return false;
        if (parameter(symbol)) {
            auto argument = resolve(given, symbol.subexpression.type, function, index, depth + 1, max_depth, fdi_length, state);
            if (argument.error) return false;
            args.push_back({argument});
        } else if (not are_equal_identifiers(symbol, given.symbols[index])) return false;
        else index++;
    }
    return true;
}

resolved_expression resolve(const expression& given, nat given_type, llvm::Function*& function,
                           nat& index, nat depth, nat max_depth, nat fdi_length, resolve_state& state) {
    
    if (not given_type or depth > max_depth) return resolution_failure;
    
    else if (index < (nat) given.symbols.size() and subexpression(given.symbols[index])) {
        auto resolved = resolve_expression(given.symbols[index].subexpression, given_type, function, state);
        index++;
        return resolved;
    }
    
    else if (index < (nat) given.symbols.size() and llvm_string(given.symbols[index]) and given_type == intrinsic::llvm) return parse_llvm_string(function, given.symbols[index].llvm.literal, index, state);
    else if (index < (nat) given.symbols.size() and llvm_string(given.symbols[index]) and given_type == intrinsic::type) return parse_llvm_type_string(function, given.symbols[index].llvm.literal, index, state);
    
    nat saved = index;
    for (auto s : state.stack.top()) {
        index = saved;
        std::vector<resolved_expression> args = {};
        if (matches(given, state.stack.get(s), given_type, args, function, index, depth, max_depth, fdi_length, state)) return {s, args};
    }
    return resolution_failure;
}

resolved_expression resolve_expression(const expression& given, nat given_type, llvm::Function*& function, resolve_state& state) {
    resolved_expression solution {};
    nat pointer = 0;
    for (nat max_depth = 0; max_depth <= max_expression_depth; max_depth++) {
        pointer = 0;
        solution = resolve(given, given_type, function, pointer, 0, max_depth, 0/*fdi_length*/, state);
        if (not solution.error and pointer == (nat) given.symbols.size()) break;
    }
    if (pointer < (nat) given.symbols.size()) solution.error = true;
    if (solution.error) printf("n3zqx2l: %s:%lld:%lld: error: unresolved expression: %s\n", state.data.file.name, given.start.line, given.start.column, expression_to_string(given, state.stack).c_str());
    return solution;
}

void symbol_table::update(llvm::ValueSymbolTable& llvm) {}
void symbol_table::push_new_frame() { frames.push_back({frames.back()}); }
void symbol_table::pop_last_frame() { frames.pop_back(); }
std::vector<nat>& symbol_table::top() { return frames.back(); }
expression& symbol_table::get(nat index) { return master[index].signature; }
void symbol_table::define(const expression& signature, const expression& definition, nat back_from, nat parent) {}

symbol_table::symbol_table(program_data& data, const std::vector<expression>& builtins) : data(data) {
    master.push_back({});  // the null entry. a type (index) of 0 means it has no type.
    for (auto signature : builtins) master.push_back({signature, {}, {}});
    std::vector<nat> compiler_intrinsics = {};
    for (nat i = 0; i < (nat) builtins.size(); i++) compiler_intrinsics.push_back(i + 1);
    frames.push_back({compiler_intrinsics});
    std::stable_sort(top().begin(), top().end(), [&](nat a, nat b) { return get(a).symbols.size() > get(b).symbols.size(); });
}

static inline void set_data_for(llvm_module& module) {
    module->setTargetTriple(llvm::sys::getDefaultTargetTriple());
    std::string lookup_error = "";
    auto target_machine = llvm::TargetRegistry::lookupTarget(module->getTargetTriple(), lookup_error)->createTargetMachine(module->getTargetTriple(), "generic", "", {}, {}, {});
    module->setDataLayout(target_machine->createDataLayout());
}

static inline llvm_module generate(expression program, const file& file, llvm::LLVMContext& context) {
    auto module = llvm::make_unique<llvm::Module>(file.name, context);
    set_data_for(module);
    llvm::IRBuilder<> builder(context);
    program_data data {file, module.get(), builder};
    symbol_table stack {data, builtins};
    resolve_state state {stack, data};
    auto main = create_main(builder, context, module);
    prune_extraneous_subexpressions(program);
    auto resolved = resolve_expression(program, intrinsic::type, main, state);
    builder.SetInsertPoint(&main->getBasicBlockList().back());
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
    if (debug) {
        debug_table(state.stack);
        print_resolved_expr(resolved, 0, state);
        module->print(llvm::outs(), nullptr);
    }
    std::string errors = "";
    if (llvm::verifyModule(*module, &(llvm::raw_string_ostream(errors) << ""))) { printf("llvm: %s: error: %s\n", file.name, errors.c_str()); return nullptr; }
    else if (resolved.error) return nullptr; else return module;
}

static inline llvm_modules frontend(const arguments& arguments, llvm::LLVMContext& context) {
    llvm::InitializeAllTargetInfos(); llvm::InitializeAllTargets(); llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers(); llvm::InitializeAllAsmPrinters();
    llvm_modules modules = {};
    for (auto file : arguments.files) modules.push_back(generate(parse(start_lex(file)), file, context)); //    std::transform(arguments.files.begin(), arguments.files.end(), std::back_inserter(modules), [&context](const file& file) { return generate(parse(file), file, context); });
    if (std::find_if(modules.begin(), modules.end(), [](llvm_module& module) { return not module; }) != modules.end()) exit(1);
    return modules;
}

static inline llvm_module link(llvm_modules&& modules) {
    auto result = std::move(modules.back()); modules.pop_back();
    for (auto& module : modules) if (llvm::Linker::linkModules(*result, std::move(module))) exit(1);
    return result;
}

static inline void interpret(llvm_module module, const arguments& arguments) {
    auto jit = llvm::EngineBuilder(std::move(module)).setEngineKind(llvm::EngineKind::JIT).create();
    jit->finalizeObject();
    exit(jit->runFunctionAsMain(jit->FindFunctionNamed("main"), {arguments.name}, nullptr));
}

static inline llvm_module optimize(llvm_module&& module) { return std::move(module); } ///TODO: unfinished.

static inline void generate_ll_file(llvm_module module, const arguments& arguments) {
    std::error_code error;
    llvm::raw_fd_ostream dest(std::string(arguments.name) + ".ll", error, llvm::sys::fs::F_None);
    if (error) exit(1);
    module->print(dest, nullptr);
}

static inline std::string generate_object_file(llvm_module module, const arguments& arguments) {
    std::string lookup_error = "";
    auto target_machine = llvm::TargetRegistry::lookupTarget(module->getTargetTriple(), lookup_error)->createTargetMachine(module->getTargetTriple(), "generic", "", {}, {}, {}); ///TODO: make this not generic!
    auto object_filename = std::string(arguments.name) + ".o";
    std::error_code error;
    llvm::raw_fd_ostream dest(object_filename, error, llvm::sys::fs::F_None);
    if (error) exit(1);
    llvm::legacy::PassManager pass;
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, llvm::TargetMachine::CGFT_ObjectFile)) { std::remove(object_filename.c_str()); exit(1); }
    pass.run(*module);
    dest.flush();
    return object_filename;
}

static inline void emit_executable(const std::string& object_file, const std::string& exec_name) {    ;
    std::system(std::string("ld -macosx_version_min 10.14 -lSystem -lc -o " + exec_name + " " + object_file).c_str());
    std::remove(object_file.c_str());
}

static inline void output(const arguments& args, llvm_module&& module) {
    if (args.output == output_type::none) interpret(std::move(module), args);
    else if (args.output == output_type::llvm) { generate_ll_file(std::move(module), args); }
    else if (args.output == output_type::assembly) { printf("cannot output .s file, unimplemented\n"); /*generate_s_file();*/ }
    else if (args.output == output_type::object_file) generate_object_file(std::move(module), args);
    else if (args.output == output_type::executable) emit_executable(generate_object_file(std::move(module), args), args.name);
}

int main(const int argc, const char** argv) {
    llvm::LLVMContext context;
    const auto args = get_arguments(argc, argv);
    output(args, optimize(link(frontend(args, context))));
}
