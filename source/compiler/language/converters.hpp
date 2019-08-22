//
//  converters.hpp
//  language
//
//  Created by Daniel Rehman on 1908213.
//  Copyright © 2019 Daniel Rehman. All rights reserved.
//

#ifndef converters_hpp
#define converters_hpp

#include "nodes.hpp"
#include "lexer.hpp"
#include "builtins.hpp"
#include "analysis_ds.hpp"
#include <string>
#include <vector>

#include "llvm/IR/Value.h" 

class symbol_table;
 
expression convert_raw_llvm_symbol_to_expression(std::string id, llvm::Value* value, symbol_table& stack, translation_unit_data& data, flags flags);
std::string expression_to_string(expression given, symbol_table& stack);
expression string_to_expression(std::string given, state& state, flags flags);

#endif /* converters_hpp */
