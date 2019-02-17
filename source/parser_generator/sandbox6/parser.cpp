/// Parsing:

static int pointer = 0;
static std::vector<node> stack_trace = {};

static int deepest_pointer = 0;
static node deepest_node = {};
static std::vector<node> deepest_stack_trace = {};

size_t deepest_level = 0;
size_t level = 0;

#define declare_node()    node self = node(__func__, {}, {}, true);                       \
int save = pointer;                                             \
stack_trace.push_back(self);                                    \
level++;                                                        \

#define params            std::vector<struct token> tokens, node &parent
#define p                 tokens, self

#define optional() level--; stack_trace.pop_back(); return begin(save, self);

#define operator_(op)     terminal(operator_type, op, p)
#define keyword_(kw)      terminal(keyword_type, kw, p)

#define b                 begin(save, self)

static bool begin(int save, node &self) {
    if (level > deepest_level) {
        deepest_level = level;
        deepest_node = self;
        deepest_stack_trace = stack_trace;
    }
    if (deepest_pointer < pointer)
        deepest_pointer = pointer;
    pointer = save;
    self.children.clear();
    return true;
}
static bool failure(int save, node &self) {
    pointer = save;
    self.children.clear();
    level--;
    stack_trace.pop_back();
    return false;
}
static bool success(node &parent, const node &self) {
    parent.children.push_back(self);
    level--;
    stack_trace.pop_back();
    return true;
}
static bool push_terminal(node &parent, std::vector<struct token> &tokens) {
    parent.children.push_back(node("terminal", tokens[pointer++], {}, true));
    if (level > deepest_level) {
        deepest_level = level;
        deepest_node = parent;
        deepest_stack_trace = stack_trace;
    }
    if (deepest_pointer < pointer)
        deepest_pointer = pointer;
    
    return true;
}

static bool terminal(enum token_type desired_token_type, std::string desired_token_value, params) {
    if (level > deepest_level) {
        deepest_level = level;
        deepest_node = parent;
        deepest_stack_trace = stack_trace;
    }
    if (deepest_pointer < pointer)
        deepest_pointer = pointer;
    
    if (pointer >= tokens.size()) return false;
    if (desired_token_type == identifier_type && tokens[pointer].type == identifier_type) return push_terminal(parent, tokens);
    if (desired_token_type == string_type && tokens[pointer].type == string_type) return push_terminal(parent, tokens);
    if (desired_token_type == documentation_type && tokens[pointer].type == documentation_type) return push_terminal(parent, tokens);
    if (desired_token_type == number_type && tokens[pointer].type == number_type) return push_terminal(parent, tokens);
    if (desired_token_type == tokens[pointer].type && desired_token_value == tokens[pointer].value) return push_terminal(parent, tokens);
    return false;
}


//this is a automatically generated parser in cpp, for my language.


bool translation_unit(params);

bool declaration_list(params);

bool interface_declaration_list(params);

bool implementation_declaration_list(params);

bool interface_declaration_block(params);

bool implementation_declaration_block(params);

bool enum_interface_declaration_block(params);

bool enum_identifier_list(params);

bool enum_implementation_declaration_block(params);

bool enum_assignment_list(params);

bool enum_assignment(params);

bool declaration(params);

bool interface_declaration(params);

bool implementation_declaration(params);

bool function_interface_declaration(params);

bool variable_interface_declaration(params);

bool type_interface_declaration(params);

bool kind_interface_declaration(params);

bool space_interface_declaration(params);

bool enum_interface_declaration(params);

bool function_implementation_declaration(params);

bool variable_implementation_declaration(params);

bool type_implementation_declaration(params);

bool kind_implementation_declaration(params);

bool space_implementation_declaration(params);

bool enum_implementation_declaration(params);

bool function_signature(params);

bool type_signature(params);

bool kind_signature(params);

bool function_call_signature(params);

bool type_call_signature(params);

bool kind_call_signature(params);

bool precedence(params);

bool qualifier_list(params);

bool return_type(params);

bool function_parameter_list(params);

bool function_parameter(params);

bool required_function_parameter(params);

bool optional_function_parameter_list(params);

bool type_parameter_list(params);

bool type_parameter(params);

bool required_type_parameter(params);

bool optional_type_parameter_list(params);

bool kind_parameter_list(params);

bool kind_parameter(params);

bool required_kind_parameter(params);

bool optional_kind_parameter_list(params);

bool parameter(params);

bool function_free_identifier(params);

bool type_free_identifier(params);

bool kind_free_identifier(params);

bool implementation_declaration_or_statement_list(params);

bool block(params);

bool statement_list(params);

bool terminated_statement(params);

bool statement(params);

bool assignment_statement(params);

bool assignment_declaration_statement(params);

bool return_statement(params);

bool for_statement(params);

bool repeatwhile_statement(params);

bool while_statement(params);

bool if_statement(params);

bool if_head_statement(params);

bool else_statement(params);

bool else_if_statement_list(params);

bool else_if_statement(params);

bool expression(params);

bool free_identifier_or_symbol_list(params);

bool symbol(params);

bool expression_list(params);

bool type_expression(params);

bool type_expression_list(params);




//EBNF Parse Nodes:

bool translation_unit(params) {
	declare_node();
	if (b && declaration_list(p)) return success(parent, self);
	if (b && newlines(p)) return success(parent, self);
	return failure(save, self);
}

bool declaration_list(params) {
	declare_node();
	if (b && newlines(p) && declaration(p) && required_newlines(p) && declaration_list(p)) return success(parent, self);
	optional();
}

bool interface_declaration_list(params) {
	declare_node();
	if (b && newlines(p) && interface_declaration(p) && required_newlines(p) && interface_declaration_list(p)) return success(parent, self);
	optional();
}

bool implementation_declaration_list(params) {
	declare_node();
	if (b && newlines(p) && implementation_declaration(p) && required_newlines(p) && implementation_declaration_list(p)) return success(parent, self);
	optional();
}

bool interface_declaration_block(params) {
	declare_node();
	if (b && operator_("{") && interface_declaration_list(p) && operator_("}")) return success(parent, self);
	return failure(save, self);
}

bool implementation_declaration_block(params) {
	declare_node();
	if (b && operator_("{") && implementation_declaration_list(p) && operator_("}")) return success(parent, self);
	return failure(save, self);
}

bool enum_interface_declaration_block(params) {
	declare_node();
	if (b && operator_("{") && enum_identifier_list(p) && operator_("}")) return success(parent, self);
	return failure(save, self);
}

bool enum_identifier_list(params) {
	declare_node();
	if (b && identifier(p) && required_newlines(p) && enum_identifier_list(p)) return success(parent, self);
	if (b && identifier(p) && operator_(",") && newlines(p) && enum_identifier_list(p)) return success(parent, self);
	optional();
}

bool enum_implementation_declaration_block(params) {
	declare_node();
	if (b && operator_("{") && enum_assignment_list(p) && operator_("}")) return success(parent, self);
	return failure(save, self);
}

bool enum_assignment_list(params) {
	declare_node();
	if (b && enum_assignment(p) && required_newlines(p) && enum_assignment_list(p)) return success(parent, self);
	if (b && enum_assignment(p) && operator_(",") && newlines(p) && enum_assignment_list(p)) return success(parent, self);
	optional();
}

bool enum_assignment(params) {
	declare_node();
	if (b && identifier(p) && operator_("=") && expression(p)) return success(parent, self);
	if (b && identifier(p)) return success(parent, self);
	return failure(save, self);
}

bool declaration(params) {
	declare_node();
	if (b && implementation_declaration(p)) return success(parent, self);
	if (b && interface_declaration(p)) return success(parent, self);
	return failure(save, self);
}

bool interface_declaration(params) {
	declare_node();
	if (b && function_interface_declaration(p)) return success(parent, self);
	if (b && type_interface_declaration(p)) return success(parent, self);
	if (b && space_interface_declaration(p)) return success(parent, self);
	if (b && enum_interface_declaration(p)) return success(parent, self);
	if (b && variable_interface_declaration(p)) return success(parent, self);
	if (b && kind_interface_declaration(p)) return success(parent, self);
	return failure(save, self);
}

bool implementation_declaration(params) {
	declare_node();
	if (b && function_implementation_declaration(p)) return success(parent, self);
	if (b && type_implementation_declaration(p)) return success(parent, self);
	if (b && space_implementation_declaration(p)) return success(parent, self);
	if (b && enum_implementation_declaration(p)) return success(parent, self);
	if (b && variable_implementation_declaration(p)) return success(parent, self);
	if (b && kind_implementation_declaration(p)) return success(parent, self);
	return failure(save, self);
}

bool function_interface_declaration(params) {
	declare_node();
	if (b && documentation(p) && function_signature(p)) return success(parent, self);
	return failure(save, self);
}

bool variable_interface_declaration(params) {
	declare_node();
	if (b && documentation(p) && assignment_declaration_statement(p)) return success(parent, self);
	return failure(save, self);
}

bool type_interface_declaration(params) {
	declare_node();
	if (b && documentation(p) && type_signature(p) && interface_declaration_block(p)) return success(parent, self);
	return failure(save, self);
}

bool kind_interface_declaration(params) {
	declare_node();
	if (b && documentation(p) && kind_signature(p)) return success(parent, self);
	return failure(save, self);
}

bool space_interface_declaration(params) {
	declare_node();
	if (b && documentation(p) && identifier(p) && declaration_block(p)) return success(parent, self);
	return failure(save, self);
}

bool enum_interface_declaration(params) {
	declare_node();
	if (b && documentation(p) && identifier(p) && operator_(":") && type_expression(p) && enum_interface_declaration_block(p)) return success(parent, self);
	if (b && documentation(p) && identifier(p) && operator_(":") && enum_interface_declaration_block(p)) return success(parent, self);
	return failure(save, self);
}

bool function_implementation_declaration(params) {
	declare_node();
	if (b && function_signature(p) && block(p)) return success(parent, self);
	return failure(save, self);
}

bool variable_implementation_declaration(params) {
	declare_node();
	if (b && assignment_statement(p)) return success(parent, self);
	return failure(save, self);
}

bool type_implementation_declaration(params) {
	declare_node();
	if (b && type_signature(p) && implementation_declaration_block(p)) return success(parent, self);
	return failure(save, self);
}

bool kind_implementation_declaration(params) {
	declare_node();
	if (b && kind_signature(p) && block(p)) return success(parent, self);
	return failure(save, self);
}

bool space_implementation_declaration(params) {
	declare_node();
	if (b && identifier(p) && implementation_declaration_block(p)) return success(parent, self);
	return failure(save, self);
}

bool enum_implementation_declaration(params) {
	declare_node();
	if (b && identifier(p) && operator_(":") && type_expression(p) && enum_implementation_declaration_block(p)) return success(parent, self);
	if (b && identifier(p) && operator_(":") && enum_implementation_declaration_block(p)) return success(parent, self);
	return failure(save, self);
}

bool function_signature(params) {
	declare_node();
	if (b && function_call_signature(p) && return_type(p) && kind_call_signature(p) && precedence(p) && qualifier_list(p)) return success(parent, self);
	return failure(save, self);
}

bool type_signature(params) {
	declare_node();
	if (b && type_call_signature(p) && parent_type(p) && kind_call_signature(p) && precedence(p) && qualfier_list(p)) return success(parent, self);
	return failure(save, self);
}

bool kind_signature(params) {
	declare_node();
	if (b && kind_call_signature(p) && return_type(p) && precedence(p) && qualifier_list(p)) return success(parent, self);
	return failure(save, self);
}

bool function_call_signature(params) {
	declare_node();
	if (b && operator_("(") && function_parameter_list(p) && operator_(")")) return success(parent, self);
	return failure(save, self);
}

bool type_call_signature(params) {
	declare_node();
	if (b && operator_("{") && type_parameter_list(p) && operator_("}")) return success(parent, self);
	return failure(save, self);
}

bool kind_call_signature(params) {
	declare_node();
	if (b && operator_("[") && kind_parameter_list(p) && operator_("]")) return success(parent, self);
	return failure(save, self);
}

bool precedence(params) {
	declare_node();
	if (b && operator_("<") && function_call_signature(p) && operator_("+") && number(p) && operator_(">")) return success(parent, self);
	if (b && operator_("<") && type_call_signature(p) && operator_("+") && number(p) && operator_(">")) return success(parent, self);
	if (b && operator_("<") && kind_call_signature(p) && operator_("+") && number(p) && operator_(">")) return success(parent, self);
	if (b && operator_("<") && number(p) && operator_(">")) return success(parent, self);
	optional();
}

bool qualifier_list(params) {
	declare_node();
	if (b && qualifier(p) && qualifier_list(p)) return success(parent, self);
	optional();
}

bool return_type(params) {
	declare_node();
	if (b && operator_("-") && operator_(">") && type_expression(p)) return success(parent, self);
	optional();
}

bool function_parameter_list(params) {
	declare_node();
	if (b && function_parameter(p) && function_parameter_list(p)) return success(parent, self);
	optional();
}

bool function_parameter(params) {
	declare_node();
	if (b && required_function_parameter(p)) return success(parent, self);
	if (b && operator_("(") && optional_function_parameter_list(p) && operator_(")")) return success(parent, self);
	return failure(save, self);
}

bool required_function_parameter(params) {
	declare_node();
	if (b && parameter(p)) return success(parent, self);
	if (b && function_free_identifier(p)) return success(parent, self);
	return failure(save, self);
}

bool optional_function_parameter_list(params) {
	declare_node();
	if (b && required_function_parameter(p) && optional_functional_parameter_list(p)) return success(parent, self);
	optional();
}

bool type_parameter_list(params) {
	declare_node();
	if (b && type_parameter(p) && type_parameter_list(p)) return success(parent, self);
	optional();
}

bool type_parameter(params) {
	declare_node();
	if (b && required_type_parameter(p)) return success(parent, self);
	if (b && operator_("(") && optional_type_parameter_list(p) && operator_(")")) return success(parent, self);
	return failure(save, self);
}

bool required_type_parameter(params) {
	declare_node();
	if (b && parameter(p)) return success(parent, self);
	if (b && type_free_identifier(p)) return success(parent, self);
	return failure(save, self);
}

bool optional_type_parameter_list(params) {
	declare_node();
	if (b && required_type_parameter(p) && optional_type_parameter_list(p)) return success(parent, self);
	optional();
}

bool kind_parameter_list(params) {
	declare_node();
	if (b && kind_parameter(p) && kind_parameter_list(p)) return success(parent, self);
	optional();
}

bool kind_parameter(params) {
	declare_node();
	if (b && required_kind_parameter(p)) return success(parent, self);
	if (b && operator_("(") && optional_kind_parameter_list(p) && operator_(")")) return success(parent, self);
	return failure(save, self);
}

bool required_kind_parameter(params) {
	declare_node();
	if (b && parameter(p)) return success(parent, self);
	if (b && kind_free_identifier(p)) return success(parent, self);
	return failure(save, self);
}

bool optional_kind_parameter_list(params) {
	declare_node();
	if (b && required_kind_parameter(p) && optional_kind_parameter_list(p)) return success(parent, self);
	optional();
}

bool parameter(params) {
	declare_node();
	if (b && identifier(p) && operator_(":") && type_expression(p) && operator_("=") && expression(p)) return success(parent, self);
	if (b && identifier(p) && operator_(":") && type_expression(p)) return success(parent, self);
	if (b && identifier(p) && operator_("=") && expression(p)) return success(parent, self);
	return failure(save, self);
}

bool function_free_identifier(params) {
	declare_node();
	if (b && overridable_keyword(p)) return success(parent, self);
	if (b && identifier(p)) return success(parent, self);
	return failure(save, self);
}

bool type_free_identifier(params) {
	declare_node();
	if (b && overridable_keyword{where(p) && x(p) && !=(p) && operator_("{") && keyword_("or") && }}(p)) return success(parent, self);
	if (b && identifier(p)) return success(parent, self);
	return failure(save, self);
}

bool kind_free_identifier(params) {
	declare_node();
	if (b && overridable_keyword{where(p) && x(p) && !=(p) && operator_("[") && keyword_("or") && ]}(p)) return success(parent, self);
	if (b && identifier(p)) return success(parent, self);
	return failure(save, self);
}

bool implementation_declaration_or_statement_list(params) {
	declare_node();
	if (b && implementation_declaration(p) && implementation_declaration_or_statement_list(p)) return success(parent, self);
	if (b && statement(p) && implementation_declaration_or_statement_list(p)) return success(parent, self);
	optional();
}

bool block(params) {
	declare_node();
	if (b && operator_("{") && statement_list(p) && operator_("}")) return success(parent, self);
	return failure(save, self);
}

bool statement_list(params) {
	declare_node();
	if (b && newlines(p) && terminated_statement(p) && statement_list(p)) return success(parent, self);
	if (b && newlines(p)) return success(parent, self);
	optional();
}

bool terminated_statement(params) {
	declare_node();
	if (b && statement{next(p) && ==(p) && operator_("}") && operator_("}")) return success(parent, self);
	if (b && statement(p) && required_newlines(p)) return success(parent, self);
	if (b && statement(p) && operator_(";")) return success(parent, self);
	return failure(save, self);
}

bool statement(params) {
	declare_node();
	if (b && if_statement(p)) return success(parent, self);
	if (b && while_statement(p)) return success(parent, self);
	if (b && repeatwhile_statement(p)) return success(parent, self);
	if (b && return_statement(p)) return success(parent, self);
	if (b && for_statement(p)) return success(parent, self);
	if (b && assignment_declaration_statement(p)) return success(parent, self);
	if (b && assignment_statement(p)) return success(parent, self);
	if (b && block(p)) return success(parent, self);
	if (b && expression(p)) return success(parent, self);
	return failure(save, self);
}

bool assignment_statement(params) {
	declare_node();
	if (b && identifier(p) && operator_(":") && type_expression(p) && operator_("=") && expression(p)) return success(parent, self);
	if (b && identifier(p) && operator_("=") && expression(p)) return success(parent, self);
	return failure(save, self);
}

bool assignment_declaration_statement(params) {
	declare_node();
	if (b && identifier(p) && operator_(":") && type_expression(p)) return success(parent, self);
	return failure(save, self);
}

bool return_statement(params) {
	declare_node();
	if (b && RETURN(p) && expression(p)) return success(parent, self);
	if (b && RETURN(p)) return success(parent, self);
	return failure(save, self);
}

bool for_statement(params) {
	declare_node();
	if (b && FOR(p) && identifier_list(p) && IN(p) && expression(p) && block(p)) return success(parent, self);
	if (b && FOR(p) && identifier_list(p) && IN(p) && expression(p) && operator_(",") && newlines(p) && statement(p)) return success(parent, self);
	return failure(save, self);
}

bool repeatwhile_statement(params) {
	declare_node();
	if (b && REPEAT(p) && block(p) && WHILE(p) && expression(p)) return success(parent, self);
	if (b && REPEAT(p) && statement(p) && WHILE(p) && expression(p)) return success(parent, self);
	return failure(save, self);
}

bool while_statement(params) {
	declare_node();
	if (b && WHILE(p) && expression(p) && block(p)) return success(parent, self);
	if (b && WHILE(p) && expression(p) && operator_(",") && newlines(p) && statement(p)) return success(parent, self);
	return failure(save, self);
}

bool if_statement(params) {
	declare_node();
	if (b && if_head_statement(p) && else_if_statement_list(p) && else_statement(p)) return success(parent, self);
	return failure(save, self);
}

bool if_head_statement(params) {
	declare_node();
	if (b && IF(p) && expression(p) && block(p)) return success(parent, self);
	if (b && IF(p) && expression(p) && operator_(",") && newlines(p) && statement(p)) return success(parent, self);
	return failure(save, self);
}

bool else_statement(params) {
	declare_node();
	if (b && newlines(p) && ELSE(p) && block(p)) return success(parent, self);
	if (b && newlines(p) && ELSE(p) && operator_(",") && newlines(p) && statement(p)) return success(parent, self);
	if (b && newlines(p) && ELSE(p) && newlines(p) && statement(p)) return success(parent, self);
	return failure(save, self);
}

bool else_if_statement_list(params) {
	declare_node();
	if (b && newlines(p) && else_if_statement(p) && else_if_statement_list(p)) return success(parent, self);
	optional();
}

bool else_if_statement(params) {
	declare_node();
	if (b && ELSE(p) && if_head_statement(p)) return success(parent, self);
	optional();
}

bool expression(params) {
	declare_node();
	if (b && free_identifier_or_symbol_list(p)) return success(parent, self);
	return failure(save, self);
}

bool free_identifier_or_symbol_list(params) {
	declare_node();
	if (b && free_identifier(p) && free_identifier_or_symbol_list(p)) return success(parent, self);
	if (b && symbol(p) && free_identifier_or_symbol_list(p)) return success(parent, self);
	optional();
}

bool symbol(params) {
	declare_node();
	if (b && operator_("(") && expression_list(p) && operator_(")")) return success(parent, self);
	if (b && operator_("[") && expression_list(p) && operator_("]")) return success(parent, self);
	if (b && operator_("{") && expression_list(p) && operator_("}")) return success(parent, self);
	if (b && number(p)) return success(parent, self);
	if (b && string(p)) return success(parent, self);
	if (b && identifier(p)) return success(parent, self);
	return failure(save, self);
}

bool expression_list(params) {
	declare_node();
	if (b && expression(p) && expression_list(p)) return success(parent, self);
	optional();
}

bool type_expression(params) {
	declare_node();
	if (b && unimplemented(p)) return success(parent, self);
	return failure(save, self);
}

bool type_expression_list(params) {
	declare_node();
	if (b && type_expression(p) && operator_(",") && type_expression_list(p)) return success(parent, self);
	if (b && free_identifier(p) && operator_(",") && type_expression_list(p)) return success(parent, self);
	if (b && type_expression(p) && type_expression_list(p)) return success(parent, self);
	if (b && free_identifier(p) && type_expression_list(p)) return success(parent, self);
	return failure(save, self);
}

