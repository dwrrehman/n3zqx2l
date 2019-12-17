// n: a n3zqx2l compiler written in C++.
#include "llvm/AsmParser/Parser.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/ValueSymbolTable.h"
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

enum class lex {none, string, identifier, llvm_string};
enum class output_type {none, llvm, assembly, object_file, executable};
enum class token_type {null, string, identifier, llvm, operator_};
enum class symbol_type { none, subexpression, string_literal, llvm_literal, identifier };
namespace intrinsic { enum intrinsic_index { typeless, type, infered, llvm, none, application, abstraction, define, evaluate }; }

struct file { const char* name = ""; std::string text = ""; };
struct arguments {
    std::vector<file> files = {};
    enum output_type output = output_type::none;
    const char* name = "";
    bool includes_standard_library = true;
};

struct location { long line = 0; long column = 0; };
struct lexing_state { long index = 0; lex state = lex::none; location at = {}; };
struct token { token_type type = token_type::null; std::string value = ""; location at = {}; };
struct string_literal { token literal = {}; bool error = 0; };
struct llvm_literal { token literal = {}; bool error = 0; };
struct identifier { token name = {}; bool error = 0; };
struct symbol;
struct expression {
    std::vector<symbol> symbols = {};
    long type = 0;
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

static long max_expression_depth = 5;
static bool debug = false;

static inline arguments get_arguments(const int argc, const char** argv) {
    arguments args = {};
    for (long i = 1; i < argc; i++) {
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

static inline bool is_identifier(char c) { return isalnum(c) or c == '_'; }
static inline bool is_close_paren(const token& t) { return t.type == token_type::operator_ and t.value == ")"; }
static inline bool is_open_paren(const token& t) { return t.type == token_type::operator_ and t.value == "("; }

static inline bool subexpression(const symbol& s) { return s.type == symbol_type::subexpression; }
static inline bool identifier(const symbol& s) { return s.type == symbol_type::identifier; }
static inline bool llvm_string(const symbol& s) { return s.type == symbol_type::llvm_literal; }
static inline bool parameter(const symbol &symbol) { return subexpression(symbol); }
static inline bool are_equal_identifiers(const symbol &first, const symbol &second) { return identifier(first) and identifier(second) and first.identifier.name.value == second.identifier.name.value; }

static inline token next(const file& file, lexing_state& lex) {
    token token = {}; auto& at = lex.index; auto& state = lex.state;
    while (lex.index < (long) file.text.size() - 1) {
        const char c = file.text[at], n = file.text[at + 1];
        if (is_identifier(c) and not is_identifier(n) and state == lex::none) { token = { token_type::identifier, std::string(1, c), lex.at}; state = lex::none; lex.at.column++; at++; return token; }
        else if (c == '\"' and state == lex::none) { token = { token_type::string, "", lex.at }; state = lex::string; }
        else if (c == '`' and state == lex::none) { token = { token_type::llvm, "", lex.at }; state = lex::llvm_string; }
        else if (is_identifier(c) and state == lex::none) { token = { token_type::identifier, std::string(1, c), lex.at }; state = lex::identifier; }
        else if (c == '\\' and state == lex::string) {
            if (n == '\\') token.value += "\\";
            else if (n == '\"') token.value += "\"";
            else if (n == 'n') token.value += "\n";
            else if (n == 't') token.value += "\t";
            else { printf("n3zqx2l: error: unknown escape sequence.\n"); exit(1); } ///TODO: make this use the standard error format.
            lex.at.column++; at++;
        } else if ((c == '\"' and state == lex::string) or (c == '`' and state == lex::llvm_string)) { state = lex::none; lex.at.column++; at++; return token; }
        else if (is_identifier(c) and not is_identifier(n) and state == lex::identifier) { token.value += c; state = lex::none; lex.at.column++; at++; return token; }
        else if (state == lex::string or state == lex::llvm_string or (is_identifier(c) and state == lex::identifier)) token.value += c;
        else if (not is_identifier(c) and not isspace(c) and state == lex::none) {
            token = { token_type::operator_, std::string(1, c), lex.at };
            state = lex::none; lex.at.column++; at++; return token;
        } if (c == '\n') { lex.at.line++; lex.at.column = 1; } else lex.at.column++; at++;
    }
    if (state == lex::string) { printf("n3zqx2l: %s:%ld,%ld: error: unterminated string\n", file.name, lex.at.line, lex.at.column); exit(1); }
    else if (state == lex::llvm_string) { printf("n3zqx2l: %s:%ld,%ld: error: unterminated llvm string\n", file.name, lex.at.line, lex.at.column); exit(1); }
    else return { token_type::null, "", lex.at };
}

static inline struct string_literal parse_string_literal(const file& file, lexing_state& state) {///TODO: make these three functions inlined into the symbol function. they are just too simple.
    auto saved = state; auto t = next(file, state);
    if (t.type != token_type::string) { state = saved; return {{}, true}; }
    return {t};
}

static inline llvm_literal parse_llvm_literal(const file& file, lexing_state& state) {
    auto saved = state; auto t = next(file, state);
    if (t.type != token_type::llvm) { state = saved; return {{}, true}; }
    return {t};
}

static inline struct identifier parse_identifier(const file& file, lexing_state& state) {
    auto saved = state; auto t = next(file, state);
    if (t.type != token_type::identifier and (is_open_paren(t) or is_close_paren(t) or t.type != token_type::operator_)) {state = saved; return {{}, true}; }
    return {t};
}

expression parse(const file& file, lexing_state& state);
static inline symbol parse_symbol(const file& file, lexing_state& state) {
    auto saved = state;
    auto open = next(file, state);
    if (is_open_paren(open)) {
        auto e = parse(file, state);
        if (not e.error) {
            auto close = next(file, state);
            if (is_close_paren(close)) return {symbol_type::subexpression, e};
            else { printf("n3zqx2l: %s:%ld:%ld: expected \")\"\n", file.name, close.at.line, close.at.column); exit(1); }
        } else state = saved;
    } else state = saved;
    if (auto string = parse_string_literal(file, state); not string.error) return {symbol_type::string_literal, {}, string}; else state = saved;
    if (auto llvm = parse_llvm_literal(file, state); not llvm.error) return {symbol_type::llvm_literal, {}, {}, llvm}; else state = saved;
    if (auto identifier = parse_identifier(file, state); not identifier.error) return {symbol_type::identifier, {}, {}, {}, identifier}; else state = saved;
    return {symbol_type::none, {}, {}, {}, {}, true};
}

expression parse(const file& file, lexing_state& state) {
    std::vector<symbol> symbols = {};
    auto saved = state; auto start = next(file, state); state = saved;
    auto symbol = parse_symbol(file, state);
    while (not symbol.error) {
        symbols.push_back(symbol);
        saved = state;
        symbol = parse_symbol(file, state);
    }
    state = saved;
    expression result = {symbols};
    result.start = start;
    return result;
}

//static inline std::string expression_to_string(const expression& given, symbol_table& stack) {
//    std::string result = "(";
//    long i = 0;
//    for (auto symbol : given.symbols) {
//        if (symbol.type == symbol_type::identifier) result += symbol.identifier.name.value;
//        else if (symbol.type == symbol_type::subexpression) result += "(" + expression_to_string(symbol.subexpression, stack) + ")";
//        if (i++ < (nat) given.symbols.size() - 1) result += " ";
//    }
//    result += ")";
//    if (given.type) result += " " + expression_to_string(stack.master[given.type].signature, stack);
//    return result;
//}

//static inline void prune_extraneous_subexpressions(expression& given) {
//    while (given.symbols.size() == 1 and subexpression(given.symbols.front())) given.symbols = given.symbols.front().subexpression.symbols;
//    for (auto& symbol : given.symbols) if (subexpression(symbol)) prune_extraneous_subexpressions(symbol.subexpression);
//}

//static inline resolved_expression parse_llvm_type_string(llvm::Function*& function, const token& llvm_string, nat& pointer, resolve_state& state) {
//    llvm::SMDiagnostic type_errors;
//    if (auto llvm_type = llvm::parseType(llvm_string.value, type_errors, *state.data.module)) {
//        pointer++; return {intrinsic::llvm, {}, llvm_type};
//    } else {
//        type_errors.print((std::string("llvm: (") + std::to_string(llvm_string.line) + "," + std::to_string(llvm_string.column) + ")").c_str(), llvm::errs());
//        return {0, {}, nullptr, {}, {}, true};
//    }
//}

//static inline resolved_expression parse_llvm_string(llvm::Function*& function, const token& llvm_string, nat& pointer, resolve_state& state) {
//    llvm::SMDiagnostic function_errors; llvm::ModuleSummaryIndex my_index(true);
//    llvm::MemoryBufferRef reference(llvm_string.value, state.data.file.name);
//    if (not llvm::parseAssemblyInto(reference, state.data.module, &my_index, function_errors)) {
//        pointer++; return {intrinsic::llvm, {}, llvm::Type::getVoidTy(state.data.module->getContext())};
//    } else {
//        function_errors.print((std::string("llvm: (") + std::to_string(llvm_string.line) + "," + std::to_string(llvm_string.column) + ")").c_str(), llvm::errs());
//        return {0, {}, nullptr, {}, {}, true};
//    }
//}

//static inline bool matches(const expression& given, const expression& signature, long given_type, std::vector<resolved_expression>& args, llvm::Function*& function,
//                           nat& index, long depth, long max_depth, long fdi_length, resolve_state& state) {
//    if (given_type != signature.type) return false;
//    for (auto symbol : signature.symbols) {
//        if (index >= (nat) given.symbols.size()) return false;
//        if (parameter(symbol)) {
//            auto argument = resolve(given, symbol.subexpression.type, function, index, depth + 1, max_depth, fdi_length, state);
//            if (argument.error) return false;
//            args.push_back({argument});
//        } else if (not are_equal_identifiers(symbol, given.symbols[index])) return false;
//        else index++;
//    }
//    return true;
//}

//resolved_expression resolve(const expression& given, long given_type, llvm::Function*& function,
//                           nat& index, long depth, long max_depth, long fdi_length, resolve_state& state) {
//
//    if (not given_type or depth > max_depth) return resolution_failure;
//
//    else if (index < (nat) given.symbols.size() and subexpression(given.symbols[index])) {
//        auto resolved = resolve_expression(given.symbols[index].subexpression, given_type, function, state);
//        index++;
//        return resolved;
//    }
//
//    else if (index < (nat) given.symbols.size() and llvm_string(given.symbols[index]) and given_type == intrinsic::llvm) return parse_llvm_string(function, given.symbols[index].llvm.literal, index, state);
//    else if (index < (nat) given.symbols.size() and llvm_string(given.symbols[index]) and given_type == intrinsic::type) return parse_llvm_type_string(function, given.symbols[index].llvm.literal, index, state);
//
//    long saved = index;
//    for (auto s : state.stack.top()) {
//        index = saved;
//        std::vector<resolved_expression> args = {};
//        if (matches(given, state.stack.get(s), given_type, args, function, index, depth, max_depth, fdi_length, state)) return {s, args};
//    }
//    return resolution_failure;
//}

//resolved_expression resolve_expression(const expression& given, long given_type, llvm::Function*& function, resolve_state& state) {
//    resolved_expression solution {};
//    long pointer = 0;
//    for (long max_depth = 0; max_depth <= max_expression_depth; max_depth++) {
//        pointer = 0;
//        solution = resolve(given, given_type, function, pointer, 0, max_depth, 0/*fdi_length*/, state);
//        if (not solution.error and pointer == (nat) given.symbols.size()) break;
//    }
//    if (pointer < (nat) given.symbols.size()) solution.error = true;
//    if (solution.error) printf("n3zqx2l: %s:%ld:%ld: error: unresolved expression: %s\n", state.data.file.name, given.start.line, given.start.column, expression_to_string(given, state.stack).c_str());
//    return solution;
//}

//void symbol_table::update(llvm::ValueSymbolTable& llvm) {}
//void symbol_table::push_new_frame() { frames.push_back({frames.back()}); }
//void symbol_table::pop_last_frame() { frames.pop_back(); }
//std::vector<nat>& symbol_table::top() { return frames.back(); }
//expression& symbol_table::get(long index) { return master[index].signature; }
//void symbol_table::define(const expression& signature, const expression& definition, long back_from, long parent) {}
//
//symbol_table::symbol_table(program_data& data, const std::vector<expression>& builtins) : data(data) {
//    master.push_back({});  // the null entry. a type (index) of 0 means it has no type.
//    for (auto signature : builtins) master.push_back({signature, {}, {}});
//    std::vector<nat> compiler_intrinsics = {};
//    for (long i = 0; i < (nat) builtins.size(); i++) compiler_intrinsics.push_back(i + 1);
//    frames.push_back({compiler_intrinsics});
//    std::stable_sort(top().begin(), top().end(), [&](long a, long b) { return get(a).symbols.size() > get(b).symbols.size(); });
//}










#define prep(x)   for (long i = 0; i < x; i++) std::cout << ".   "


void debug_arguments(const arguments& args) {
    std::cout << "file count = " <<  args.files.size() << "\n";
    for (auto a : args.files) {
        std::cout << "file: " << a.name << "\n";
        std::cout << "data: \n:::" << a.text << ":::\n";
    }
    std::cout << "exec name = " << args.name << std::endl;
}


const char* convert_token_type_representation(enum token_type type) {
    switch (type) {
        case token_type::null: return "{null}";
        case token_type::string: return "string";
        case token_type::identifier: return "identifier";
        case token_type::operator_: return "operator";
        case token_type::llvm: return "llvm";
    }
}

void print_lex(const std::vector<struct token>& tokens) {
    std::cout << "::::::::::LEX:::::::::::" << std::endl;
    for (auto token : tokens) {
        std::cout << "TOKEN(type: " << convert_token_type_representation(token.type) << ", value: \"" << (token.value != "\n" ? token.value : "\\n") << "\", [" << token.at.line << ":" << token.at.column << "])" << std::endl;
    }
    std::cout << ":::::::END OF LEX:::::::" << std::endl;
}


void debug_token_stream(const file& file) {
    std::vector<struct token> tokens = {};
    struct token t = {};
    lexing_state state = {0, lex::none, {1, 1}};
    while ((t = next(file, state)).type != token_type::null) tokens.push_back(t);
    print_lex(tokens);
}

void print_expression(expression s, int d);


void print_symbol(symbol symbol, int d) {
    prep(d); std::cout << "symbol: \n";
    switch (symbol.type) {

        case symbol_type::identifier:
            prep(d); std::cout << convert_token_type_representation(symbol.identifier.name.type) << ": " << symbol.identifier.name.value << "\n";
            break;

        case symbol_type::llvm_literal:
            prep(d); std::cout << "llvm literal: \'" << symbol.llvm.literal.value << "\'\n";
            break;

        case symbol_type::string_literal:
            prep(d); std::cout << "string literal: \"" << symbol.string.literal.value << "\"\n";
            break;
            
        case symbol_type::subexpression:
            prep(d); std::cout << "list symbol\n";
            print_expression(symbol.subexpression, d+1);
            break;
        
        case symbol_type::none:
            prep(d); std::cout << "{NO SYMBOL TYPE}\n";
            break;
        default: break;
    }
}

void print_expression(expression expression, int d) {
    prep(d); std::cout << "expression: \n";
    prep(d); std::cout << std::boolalpha << "error: " << expression.error << "\n";
    prep(d); std::cout << "symbol count: " << expression.symbols.size() << "\n";
    prep(d); std::cout << "symbols: \n";
    int i = 0;
    for (auto symbol : expression.symbols) {
        prep(d+1); std::cout << i << ": \n";
        print_symbol(symbol, d+1);
        std::cout << "\n";
        i++;
    }
    
    prep(d); std::cout << "type = " << expression.type << "\n";
}

void print_translation_unit(expression unit, const file& file) {
    std::cout << "translation unit: (" << file.name << ")\n";
    print_expression(unit, 1);
}

std::string convert_symbol_type(enum symbol_type type) {
    switch (type) {
        case symbol_type::none:
            return "{none}";
        case symbol_type::subexpression:
            return "subexpression";
        case symbol_type::string_literal:
            return "string literal";
        case symbol_type::llvm_literal:
            return "llvm literal";
        case symbol_type::identifier:
            return "identifier";
    }
}























using stack = std::vector<llvm::ValueSymbolTable>;

static inline void set_data_for(std::unique_ptr<llvm::Module>& module) {
    module->setTargetTriple(llvm::sys::getDefaultTargetTriple());
    std::string lookup_error = "";
    auto target_machine = llvm::TargetRegistry::lookupTarget(module->getTargetTriple(), lookup_error)->createTargetMachine(module->getTargetTriple(), "generic", "", {}, {}, {});
    module->setDataLayout(target_machine->createDataLayout());
}




static void print_stack(const stack &stack) {
    std::cout << "-----------------------printing stack....--------------------------------\n";
    for (auto frame : stack) {
        std::cout << "-------------- printing new frame: ------------\n";
        for (auto& entry : frame) {
            std::string key = entry.getKey();
            llvm::Value* value = entry.getValue();
            
            std::cout << "key: \""<<key<<"\" == value : \n";
            value->print(llvm::outs());
            std::cout << "\n\n";
        }
    }
}

static void parse_ll_file(const program_data &data, const file &file) {
    llvm::SMDiagnostic function_errors; llvm::ModuleSummaryIndex my_index(true);
    llvm::MemoryBufferRef reference(file.text, file.name);
    
    if (not llvm::parseAssemblyInto(reference, data.module, &my_index, function_errors)) {
        printf("llvm parse assembly into:  success!\n");
    } else {
        function_errors.print("llvm: ", llvm::errs());
    }
}

static void open_ll_file(const char *core_name, struct file &core_stdlib) {
    std::ifstream stream {core_name};
    if (stream.good()) {
        std::string text {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        stream.close();
        core_stdlib = {core_name, text};
    } else { printf("n: error: unable to open \"%s\": %s\n", core_name, strerror(errno)); exit(1); }
}

//static inline resolved_expression parse_llvm_string(llvm::Function*& function, const token& llvm_string, nat& pointer, resolve_state& state) {
//    llvm::SMDiagnostic function_errors; llvm::ModuleSummaryIndex my_index(true);
//    llvm::MemoryBufferRef reference(llvm_string.value, state.data.file.name);
//    if (not llvm::parseAssemblyInto(reference, state.data.module, &my_index, function_errors)) {
//        pointer++; return {intrinsic::llvm, {}, llvm::Type::getVoidTy(state.data.module->getContext())};
//    } else {
//        function_errors.print((std::string("llvm: (") + std::to_string(llvm_string.line) + "," + std::to_string(llvm_string.column) + ")").c_str(), llvm::errs());
//        return {0, {}, nullptr, {}, {}, true};
//    }
//}

static inline std::unique_ptr<llvm::Module> generate(expression program, const file& file, llvm::LLVMContext& context) {
    
    auto module = llvm::make_unique<llvm::Module>(file.name, context);
    set_data_for(module);
    llvm::IRBuilder<> builder(context);
    program_data data {file, module.get(), builder};
    stack stack {};
    
    auto main = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context), {llvm::Type::getInt32Ty(context), llvm::Type::getInt8PtrTy(context)->getPointerTo()}, false), llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", main));
        
    
    const char* core_name = "/Users/deniylreimn/Documents/projects/n3zqx2l/examples/core.ll";
    struct file core_stdlib = {};
    open_ll_file(core_name, core_stdlib);
    parse_ll_file(data, core_stdlib);
    
    
    
    
    
    
    auto wef = module->getValueSymbolTable();
    auto wef2 = module->getValueSymbolTable();
    stack.push_back(wef);
    stack.push_back(wef2);
    
    print_stack(stack);
    
//    auto resolved = resolve_expression(program, intrinsic::type, main, stack);
    
    builder.SetInsertPoint(&main->getBasicBlockList().back());
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
        
    module->print(llvm::outs(), nullptr);
    
    std::string errors = "";
    if (llvm::verifyModule(*module, &(llvm::raw_string_ostream(errors) << ""))) { printf("llvm: %s: error: %s\n", file.name, errors.c_str()); return nullptr; }
//    else if (resolved.error) return nullptr;
    return module;
}


static inline std::vector<std::unique_ptr<llvm::Module>> frontend(const arguments& arguments, llvm::LLVMContext& context) {
    llvm::InitializeAllTargetInfos(); llvm::InitializeAllTargets(); llvm::InitializeAllTargetMCs(); llvm::InitializeAllAsmParsers(); llvm::InitializeAllAsmPrinters();
    std::vector<std::unique_ptr<llvm::Module>> modules = {};
    for (auto file : arguments.files) {
        lexing_state state {0, lex::none, {1, 1}};
        
        auto saved = state;
        debug_token_stream(file);
        print_translation_unit(parse(file, state), file);
        state = saved;
        
        modules.push_back(generate(parse(file, state), file, context));
    }
    if (std::find_if(modules.begin(), modules.end(), [](auto& module) { return not module; }) != modules.end()) exit(1);
    return modules;
}

static inline std::unique_ptr<llvm::Module> link(std::vector<std::unique_ptr<llvm::Module>>&& modules) {
    auto result = std::move(modules.back()); modules.pop_back();
    for (auto& module : modules) if (llvm::Linker::linkModules(*result, std::move(module))) exit(1);
    return result;
}

static inline void interpret(std::unique_ptr<llvm::Module> module, const arguments& arguments) {
    auto jit = llvm::EngineBuilder(std::move(module)).setEngineKind(llvm::EngineKind::JIT).create();
    jit->finalizeObject();
    exit(jit->runFunctionAsMain(jit->FindFunctionNamed("main"), {arguments.name}, nullptr));
}

static inline std::unique_ptr<llvm::Module> optimize(std::unique_ptr<llvm::Module>&& module) { return std::move(module); } ///TODO: unfinished.

static inline void generate_ll_file(std::unique_ptr<llvm::Module> module, const arguments& arguments) {
    std::error_code error;
    llvm::raw_fd_ostream dest(std::string(arguments.name) + ".ll", error, llvm::sys::fs::F_None);
    if (error) exit(1);
    module->print(dest, nullptr);
}

static inline std::string generate_object_file(std::unique_ptr<llvm::Module> module, const arguments& arguments) {
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

static inline void output(const arguments& args, std::unique_ptr<llvm::Module>&& module) {
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
