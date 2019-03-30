//
//  nodes.hpp
//  language
//
//  Created by Daniel Rehman on 1902284.
//  Copyright © 2019 Daniel Rehman. All rights reserved.
//

#ifndef nodes_hpp
#define nodes_hpp

#include "lexer.hpp"

#include <string>
#include <vector>

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"


/* -------------------------- EBNF GRAMMAR FOR THE LANGUAGE: ----------------------------


 translation_unit
 = expression_list

 expression_list
 = newlines terminated_expression expression_list
 | E

 terminated_expression
 = expression required_newlines

 function_signature
 = call_signature return_type signature_type

 variable_signature
 = element_list signature_type

 call_signature
 = ( element_list )

 element_list
 = element element_list
 | E

 element
 = symbol signature_type

 return_type
 = expression
 | E

 signature_type
 = : expression
 | E

 expression
 = symbol expression
 | symbol

 newlines_expression
 = symbol newlines expression
 = symbol

 symbol
 = function_signature
 | variable_signature
 | ( newlines newlines_expression )
 | string_literal
 | character_literal
 | documentation
 | llvm_literal
 | block
 | group
 | builtin
 | identifier

 block
 = { expression_list }
 | {{ expression_list }}
 | { expression }
 | {{ expression }}

 ///| newlines expression          ; this is problematic. we will do this as a correction transformation.

 group
 = ( expression_list )
 = ( expression )



----------------------------------------------------------------------------*/

// base class for all ast nodes:

class node {
public:
    bool error = true;
};

// enum classes:

enum class symbol_type {
    none,
    function_signature,
    variable_signature,
    subexpression,
    string_literal,
    character_literal,
    documentation,
    llvm_literal,
    block,
    group,
    builtin,
    identifier,
};

// prototypes:

class translation_unit;
class expression_list;
class terminated_expression;
class expression;
class newlines_expression;
class symbol;
class function_signature;
class variable_signature;
class block;
class group;
class element_list;
class element;
class return_type;
class signature_type;

// literals:

class string_literal: public node {
public:
    struct token literal = {};
};

class character_literal: public node {
public:
    struct token literal = {};
};

class documentation: public node {
public:
    struct token literal = {};
};

class llvm_literal: public node {
public:
    struct token literal = {};
};

class identifier: public node {
public:
    struct token name = {};
};

class builtin: public node {
public:
    struct token name = {};
};

class expression: public node {
public:
    std::vector<symbol> symbols = {};
};

class expression_list: public node {
public:
    std::vector<expression> expressions = {};
};

class element_list: public node {
public:
    std::vector<element> elements = {};
};

class block: public node {
public:
    bool is_open = false;
    bool is_cloed = false;
    expression_list statements = {};
};

class group: public node {
public:
    expression_list declarations = {};
};

class function_signature: public node {
public:
    bool has_return_type = false;
    bool has_signature_type = false;
    element_list call = {};
    expression return_type = {};
    expression signature_type = {};
};

class variable_signature: public node {
public:
    element_list name = {};
    expression signature_type = {};
};

class symbol: public node {
public:
    enum symbol_type type = symbol_type::none;
    function_signature function = {};
    variable_signature variable = {};
    expression subexpression = {};
    block block = {};
    group group = {};
    string_literal string = {};
    character_literal character = {};
    documentation documentation = {};
    llvm_literal llvm = {};
    builtin builtin = {};
    identifier identifier = {};
};

class element: public node {
public:
    bool has_type = false;
    symbol name = {};
    expression type = {};
};

class translation_unit: public node {
public:
    expression_list list = {};
};

#endif /* nodes_hpp */
