

////////////////////////////////// DEBUG CODE ////////////////////////////////////

void prep(N d) { for (N i = 0; i < d; i++) printf(".   "); }

static inline void debug_line_expression(const e& given) {
    if (given.t.v == 1) printf("\"%s\"", given.t.s);
    else if (given.t.v) putchar(given.t.v);
    else { putchar('('); for (const auto& symbol : given.s) debug_line_expression(symbol); putchar(')'); }
}

static inline void debug_expression(e e, N d) {
    if (not debug) return;
    prep(d); printf("error = %lu\n", e.e);
    prep(d); printf("token: %lu : '%c'  @(%lu, %lu)\n", e.t.v, (char)e.t.v, e.t.l, e.t.c);
    if (e.t.v == 1) { prep(d); printf("    string = %s\n", e.t.s); }
    prep(d); printf("children: {\n");
    for (auto f : e.s) {
        prep(d+1); printf("child: \n");
        debug_expression(f, d + 1);
    }
    prep(d); printf("}\n");
}
static inline void debug_intrinsics(V intrinsics) {
    if (not debug) return;
    printf("\n---- debugging intrinsics: ----\n");
    for (N i = 0; i < intrinsics.size(); i++) {
        if (intrinsics[i].empty()) continue;
        printf("\t ----- INTRINSIC ID # %lu ---- \n\t\tsignatures: { ", i);
        for (auto index : intrinsics[i]) printf("%lu ",index);
        printf("}\n\n");
    }
    printf("\n--------------------------------\n");
}

static inline void debug_resolved(r e, W entries, N depth = 0) {
    if (not debug) return;
    if (e.e) { prep(depth); printf("[ERROR]\n"); }
    prep(depth); printf("%lu :   index = %lu  ::   %s\n", depth, e.i, sr({e.i}, entries).c_str());
    if (e.t.v) { prep(depth); printf("literal:   %lu : '%c'  @(%lu, %lu)\n", e.t.v, (char)e.t.v, e.t.l, e.t.c); }
    prep(depth); puts("");
    N i = 0;
    for (auto arg : e.a) {
        prep(depth + 1); printf("#%lu: \n", i++);
        debug_resolved(arg, entries, depth + 1);
    }
}

static void debug_lex(const F &file) {
    if (not debug) return;
    
    printf("-------------- lexing --------------\n");
    L temp_state {0, 0, 1, 1};
    
    struct F temp_file = {file.n, (char*) calloc(file.l, sizeof(char)), file.l};
    for (N i = 0; i < file.l; i++) temp_file.t[i] = file.t[i];
    
    t t = n(temp_state, temp_file);
    while (t.v) {
        
        printf("token: %lu : '%c'  @(%lu, %lu)", t.v, (char)t.v, t.l, t.c);
        
        if (t.v == 1) printf(" : string: %s\n", t.s);
        else printf("\n");
        
        t = n(temp_state, temp_file);
    }
}

static inline void debug_stack(W entries, V stack) {
    if (not debug) return;
    
    printf("\n---- debugging stack: ----\n");
    printf("printing frames: \n");
    
    for (N i = 0; i < stack.size(); i++) {
        
        printf("\t ----- FRAME # %lu ---- \n\t\tidxs: { ", i);
        
        for (auto index : stack[i]) printf("%lu ", index);
        
        puts("}");
    }
    printf("\nmaster: {\n");
    
    N j = 0;
    for (auto entry : entries) {
        printf("\t%6lu: ", j);
        
        printf("%s", sr({j}, entries).c_str());
                
        auto def = sr(entry.d, entries);
        if (def.size()) printf("      =    %s", def.c_str());
        
        puts("\n");
        j++;
    }
    puts("}");
}
////////////////////////////////// DEBUG CODE ////////////////////////////////////













if (debug) {
    printf("\n\ndebugging resolved:\n\n");
    debug_resolved(g, E);
    printf("\n\nrepresenting resolved:\n\n\t");
    puts(sr(g, E).c_str());
    printf("\n\nprinting entries/stack:\n\n");
    debug_stack(E, S);
    printf("\n\nprinting intrinsics\n\n");
    debug_intrinsics(I);
    printf("\n\n");
}


debug_stack(E, S); debug_lex(f);












//                std::vector<std::string> defined_intrinsics {
//                    "(i)",
//                    "(name) (i)",
//                    "(nat) (i)",
//                    "(join ((join-first) (i)) ((join-second) (i))) (i)",
//                    "(decl ((decl-name) (name) (i)) ((decl-ii) (nat) (i)) ((decl-extern) (nat) (i))) (i)"
//                };
                            
//        if (is_intrin(_declare, s, intrinsics) and args[1].number < _intrinsic_count) intrinsics[args[1].number].push_back(args[0].name[0].me.index = define(args[0].name[0], {}, entries, stack));
//        if (is_intrin(_define, s, intrinsics)) args[0].name[0].me.index = define(args[0].name[0], args[2], entries, stack);

///TODO: we need to throw an error when we encounter a unexpected ")". very important, actually.    at any cost, we need to do it.    it is a silent but deadly nonerror right now.   because it can lead to code not beinginterretered at all.




llvm::IRBuilder<> b(C);
sd(m);
if (g.e) {
    if (debug) printf("error: not generating llvm: resolution error\n");
    return m;
}
auto main = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(C), {
    llvm::Type::getInt32Ty(C),
    llvm::Type::getInt8PtrTy(C)->getPointerTo()
}, false), llvm::Function::ExternalLinkage, "main", m.get());
b.SetInsertPoint(llvm::BasicBlock::Create(C, "entry", main));
auto v = generate_expression(g, E, S, m.get(), main, b);
if (debug) {
    printf("the top level value was found to be: \n");
    if (v) v->print(llvm::outs());
    else printf("(null)");
    printf("\n");
}
b.SetInsertPoint(&main->getBasicBlockList().back());
b.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(C), 0));
if (debug) {
    printf(" ------------generating code for module ----------:\n\n");
    m->print(llvm::outs(), nullptr);
}
std::string er = "";
if (llvm::verifyModule(*m, &(llvm::raw_string_ostream(er) << ""))) printf("llvm: %s: error: %s\n", f.n, er.c_str());
