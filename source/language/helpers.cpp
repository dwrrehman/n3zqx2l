//
//  helpers.cpp
//  language
//
//  Created by Daniel Rehman on 1908213.
//  Copyright © 2019 Daniel Rehman. All rights reserved.
//

#include "helpers.hpp"

#include "analysis_ds.hpp"
#include "compiler.hpp"
#include "parser.hpp"
#include "builtins.hpp"
#include "symbol_table.hpp"
#include "lists.hpp"
#include "error.hpp"
#include  "llvm_parser.hpp"

#include "llvm/IR/LLVMContext.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/ModuleSummaryIndex.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/daniels_interpreter/MCJIT.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/GenericValue.h"

#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

const resolved_expression resolution_failure = {0, {}, true};
const resolved_expression resolved_unit_value = {intrin::empty, {}, false};

bool subexpression(const symbol& s) { return s.type == symbol_type::subexpression; }
bool identifier(const symbol& s) { return s.type == symbol_type::identifier; }
bool llvm_string(const symbol& s) { return s.type == symbol_type::llvm_literal; }
bool parameter(const symbol &symbol) { return subexpression(symbol); }


bool contains_final_terminator(llvm::Function* main_function) {
    auto& blocks = main_function->getBasicBlockList();
    if (blocks.size()) {
        auto& instructions = blocks.back().getInstList();
        auto& last = instructions.back();
        if (last.isTerminator()) return true;
    }
    return false;
}

void append_return_0_statement(llvm::IRBuilder<> &builder, llvm::Function* main_function, llvm::LLVMContext &context) {
    llvm::Value* value = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);    
    builder.SetInsertPoint(&main_function->getBasicBlockList().back());
    builder.CreateRet(value);
}

void call_donothing(llvm::IRBuilder<> &builder, const std::unique_ptr<llvm::Module> &module) {
    llvm::Function* donothing = llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::donothing);        
    builder.CreateCall(donothing);
}

llvm::Function* create_main(llvm::IRBuilder<>& builder, llvm::LLVMContext& context, const std::unique_ptr<llvm::Module>& module) {
    std::vector<llvm::Type*> state = {llvm::Type::getInt32Ty(context), llvm::Type::getInt8PtrTy(context)->getPointerTo()};
    llvm::FunctionType* main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), state, false);
    llvm::Function* main_function = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", main_function));    
    return main_function;
}

void prune_extraneous_subexpressions_in_expression_list(expression_list& given);
void prune_extraneous_subexpressions_in_expression(expression& given);

void prune_extraneous_subexpressions_in_expression(expression& given) { // unimplemented
    while (given.symbols.size() == 1
           and subexpression(given.symbols[0])
           and given.symbols[0].expressions.list.size() == 1
           and given.symbols[0].expressions.list.back().symbols.size()) {        
        auto save = given.symbols[0].expressions.list.back().symbols;
        given.symbols = save;
    }
    for (auto& symbol : given.symbols)
        if (subexpression(symbol)) prune_extraneous_subexpressions_in_expression_list(symbol.expressions);
}

void prune_extraneous_subexpressions_in_expression_list(expression_list& given) {
    for (auto& expression : given.list)
        prune_extraneous_subexpressions_in_expression(expression);
}


static std::vector<llvm::GenericValue> turn_into_value_array(std::vector<nat> canonical_arguments, std::unique_ptr<llvm::Module>& module) {
    std::vector<llvm::GenericValue> arguments = {};
    for (auto index : canonical_arguments) 
        arguments.push_back(llvm::GenericValue {llvm::ConstantInt::get(llvm::Type::getInt32Ty(module->getContext()), index)});
    return arguments;
}

nat evaluate(std::unique_ptr<llvm::Module>& module, llvm::Function* function, std::vector<nat> args) {    
    set_data_layout(module);
    auto jit = llvm::EngineBuilder(std::unique_ptr<llvm::Module>{module.get()}).setEngineKind(llvm::EngineKind::JIT).create();
    jit->finalizeObject();
    return jit->runFunction(function, turn_into_value_array(args, module)).IntVal.getLimitedValue();
}

bool are_equal_identifiers(const symbol &first, const symbol &second) {
    return identifier(first) and identifier(second) 
    and first.identifier.name.value == second.identifier.name.value;
}

static bool matches(expression given, expression signature, nat given_type, std::vector<resolved_expression_list>& args, llvm::Function*& function, nat& index, const nat depth, const nat max_depth, state& state, flags flags) {    
    if (given_type != signature.type and given.type != intrin::infered) return false;
    for (auto symbol : signature.symbols) {
        if (index >= given.symbols.size()) return false;
        
        if (parameter(symbol) and subexpression(given.symbols[index])) {
            auto argument = resolve_expression_list(given.symbols[index].expressions, symbol.expressions.list.back().type, function, state, flags);            
            if (argument.error) return false;
            args.push_back(argument);
            index++;
            
        } else if (parameter(symbol)) {
            
            auto argument = resolve(given, symbol.expressions.list.back().type, function, index, depth + 1, max_depth, 0/*temp*/, state, flags); 
            // temp
            
            if (argument.error) return false;
            args.push_back({{argument}});            
            
        } else if (not are_equal_identifiers(symbol, given.symbols[index])) return false;
        else index++;
    }
    return true;
}

resolved_expression resolve(expression given, nat given_type, llvm::Function*& function, 
                            nat& index, nat depth, nat max_depth, nat fdi_length,
                            state& state, flags flags) {

    if (index >= given.symbols.size() or not given_type or depth > max_depth) return resolution_failure;
    if (llvm_string(given.symbols[index]) and given_type == intrin::unit)
        return parse_llvm_string(given, function, given.symbols[index].llvm.literal.value, index, state, flags);
        
    if (given_type == intrin::abstraction) {
        index = fdi_length; // is this right?
        
    }
    
    size_t saved = index;
    for (auto signature_index : state.stack.top()) {
        index = saved;
        std::vector<resolved_expression_list> args = {};
        if (matches(given, state.stack.get(signature_index), given_type, args, function, index, depth, max_depth, state, flags)) 
            return {signature_index, args, false};
    }
    return resolution_failure;
}

static void print_unresolved_error(const expression &given, state &state) {
    const std::string name = expression_to_string(given, state.stack);
    print_error_message(state.data.file.name, "unresolved expression: " + name, given.starting_token.line, given.starting_token.column);
    print_source_code(state.data.file.text, {given.starting_token});    
}

resolved_expression resolve_expression(expression given, nat given_type, llvm::Function*& function, state& state, flags flags) {
    

    
    if (given_type == intrin::abstraction) {
        
        
        resolved_expression solution {};
        nat pointer = 0;
        
        for (nat max_depth = 0; max_depth <= max_expression_depth; max_depth++) {

            for (nat fdi_length = given.symbols.size(); fdi_length--;) { 
            
                pointer = 0;
                solution = resolve(given, given_type, function, pointer, 0, max_depth, fdi_length, state, flags);
                if (not solution.error and pointer == given.symbols.size()) break;
                
            }
            
        }     
        if (pointer < given.symbols.size()) solution.error = true; 
        if (solution.error) print_unresolved_error(given, state);
        return solution;
        
    }
    
    resolved_expression solution {};
    nat pointer = 0;
    for (nat max_depth = 0; max_depth <= max_expression_depth; max_depth++) {            
        pointer = 0;
        solution = resolve(given, given_type, function, pointer, 0, max_depth, 0, state, flags);
        if (not solution.error and pointer == given.symbols.size()) break;
    }     
    if (pointer < given.symbols.size()) solution.error = true; 
    if (solution.error) print_unresolved_error(given, state);
    return solution;
}

static std::vector<nat> generate_type_list(const expression_list &given, nat given_type) {
    if (given.list.empty()) return {};
    std::vector<nat> types (given.list.size() - 1, intrin::unit);
    types.push_back(given_type);
    return types;
}

static bool is_unit_value(expression &expression) {
    return expression.symbols.size() == 1 
    and subexpression(expression.symbols[0]) 
    and expression.symbols[0].expressions.list.empty();
}

resolved_expression_list resolve_expression_list(expression_list given, nat given_type, llvm::Function*& function, state& state, flags flags) {
    if (given.list.empty()) return {{resolved_unit_value}, given_type != intrin::unit};
    nat i = 0;
    auto types = generate_type_list(given, given_type);    
    resolved_expression_list solutions {};
    for (auto expression : given.list) {
        auto type = types[i++];
        
        if (is_unit_value(expression) and type == intrin::unit) solutions.list.push_back(resolved_unit_value);   
        ////TODO push this into the resoled expresiion function.
        
        else solutions.list.push_back(resolve_expression(expression, type, function, state, flags));        
    }
    for (auto e : solutions.list) solutions.error |= e.error;    
    return solutions;
}

std::string emit(const std::unique_ptr<llvm::Module>& module) {
    std::string string = "";
    module->print(llvm::raw_string_ostream(string) << "", NULL); 
    return string;
}


static bool is_donothing_call(llvm::Instruction* ins) {   
    return ins 
    and std::string(ins->getOpcodeName()) == "call" 
    and std::string(ins->getOperand(0)->getName()) == "llvm.donothing";
}

static bool is_unreachable_instruction(llvm::Instruction* ins) {
    return ins 
    and std::string(ins->getOpcodeName()) == "unreachable";
}

void delete_empty_blocks(std::unique_ptr<llvm::Module> &module) { 
    for (auto& function : module->getFunctionList()) {        
        llvm::SmallVector<llvm::BasicBlock*, 100> blocks = {};        
        for (auto& block : function.getBasicBlockList()) {
            if (block.empty()) blocks.push_back(&block);
        }
        llvm::DeleteDeadBlocks(blocks);
    }
}


void move_lone_terminators_into_previous_blocks(std::unique_ptr<llvm::Module>& module) {
    for (auto& function : module->getFunctionList()) {        
        llvm::BasicBlock* previous = nullptr;
        
        for (auto& block : function.getBasicBlockList()) {            
            auto& instructions = block.getInstList();
            
            if (previous and instructions.size() == 1 
                and instructions.back().isTerminator())
                instructions.back().moveAfter(&previous->getInstList().back());
            
            previous = &block;
        }
    }
    
    ///KNOWN BUG: when we have no terminators in sight, this function 
    /// does remove the unneccessary basic block which is put between the 
    /// bits of code which should be in the same block.
    
}

void remove_extraneous_insertion_points_in(std::unique_ptr<llvm::Module>& module) {    
    for (auto& function : module->getFunctionList()) {
        for (auto& block : function.getBasicBlockList()) {    
            auto terminator = block.getTerminator();
            if (!terminator) continue;
            auto previous = terminator->getPrevNonDebugInstruction();                        
            if (is_unreachable_instruction(terminator) and is_donothing_call(previous)) {
                previous->eraseFromParent();                 
                terminator->eraseFromParent();
            }
        }
    }    
}