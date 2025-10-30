#include "cminusf_builder.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

bool promote(IRBuilder *builder, Value **l_val_p, Value **r_val_p) {
    bool is_int = false;
    auto &l_val = *l_val_p;
    auto &r_val = *r_val_p;
    if (l_val->get_type() == r_val->get_type()) {
        is_int = l_val->get_type()->is_integer_type();
    } else {
        if (l_val->get_type()->is_integer_type()) {
            l_val = builder->create_sitofp(l_val, FLOAT_T);
        } else {
            r_val = builder->create_sitofp(r_val, FLOAT_T);
        }
    }
    return is_int;
}

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

Value* CminusfBuilder::visit(ASTProgram &node) {
    VOID_T = module->get_void_type();
    INT1_T = module->get_int1_type();
    INT32_T = module->get_int32_type();
    INT32PTR_T = module->get_int32_ptr_type();
    FLOAT_T = module->get_float_type();
    FLOATPTR_T = module->get_float_ptr_type();

    Value *ret_val = nullptr;
    for (auto &decl : node.declarations) {
        ret_val = decl->accept(*this);
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTNum &node) {
    if (node.type == TYPE_INT) {
        return CONST_INT(node.i_val);
    }
    return CONST_FP(node.f_val);
}

Value* CminusfBuilder::visit(ASTVarDeclaration &node) {
    // 判定基本类型
    Type *elem_ty = (node.type == TYPE_INT) ? INT32_T : FLOAT_T;

    bool is_global = (context.func == nullptr);
    if (!node.num) {
        // 标量
        if (is_global) {
            // 全局：必须零初始化
            auto gv = GlobalVariable::create(
                node.id, module.get(), elem_ty, /*is_const=*/false,
                ConstantZero::get(elem_ty, module.get()));
            scope.push(node.id, gv);
            return gv;
        } else {
            // 局部：alloca
            auto addr = builder->create_alloca(elem_ty);
            scope.push(node.id, addr);
            return addr;
        }
    } else {
        // 数组
        auto num = node.num->i_val; // 语法保证是整型字面量
        auto arr_ty = ArrayType::get(elem_ty, num);

        if (is_global) {
            // 全局数组：零初始化
            auto gv = GlobalVariable::create(
                node.id, module.get(), arr_ty, /*is_const=*/false,
                ConstantZero::get(arr_ty, module.get()));
            scope.push(node.id, gv);
            return gv;
        } else {
            // 局部数组：alloca 数组类型
            auto addr = builder->create_alloca(arr_ty);
            scope.push(node.id, addr);
            return addr;
        }
    }
}

Value* CminusfBuilder::visit(ASTFunDeclaration &node) {
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    for (auto &param : node.params) {
        if (param->type == TYPE_INT) {
            if (param->isarray) {
                param_types.push_back(INT32PTR_T);
            } else {
                param_types.push_back(INT32_T);
            }
        } else {
            if (param->isarray) {
                param_types.push_back(FLOATPTR_T);
            } else {
                param_types.push_back(FLOAT_T);
            }
        }
    }

    fun_type = FunctionType::get(ret_type, param_types);
    auto func = Function::create(fun_type, node.id, module.get());
    scope.push(node.id, func);
    context.func = func;
    auto funBB = BasicBlock::create(module.get(), "entry", func);
    builder->set_insert_point(funBB);
    scope.enter();
    context.pre_enter_scope = true;
    std::vector<Value *> args;
    for (auto &arg : func->get_args()) {
        args.push_back(&arg);
    }
    for (unsigned int i = 0; i < node.params.size(); ++i) {
        auto *param_i = node.params[i]->accept(*this);
        args[i]->set_name(node.params[i]->id);
        builder->create_store(args[i], param_i);
        scope.push(args[i]->get_name(), param_i);
    }
    node.compound_stmt->accept(*this);

    // 确保函数有返回语句
    if (builder->get_insert_block() &&
        !builder->get_insert_block()->is_terminated()) {
        if (context.func->get_return_type()->is_void_type()) {
            builder->create_void_ret();
        } else if (context.func->get_return_type()->is_float_type()) {
            builder->create_ret(CONST_FP(0.));
        } else {
            builder->create_ret(CONST_INT(0));
        }
    }

    scope.exit();
    return nullptr;
}

Value *CminusfBuilder::visit(ASTParam &node) {
    if (node.isarray) {
        // 数组参数：存放成指针
        Type *ptr_ty = (node.type == TYPE_INT) ? INT32PTR_T : FLOATPTR_T;
        return builder->create_alloca(ptr_ty);
    } else {
        // 标量参数：存放成值
        Type *val_ty = (node.type == TYPE_INT) ? INT32_T : FLOAT_T;
        return builder->create_alloca(val_ty);
    }
}

Value *CminusfBuilder::visit(ASTCompoundStmt &node) {
    bool need_enter = true;
    if (context.pre_enter_scope) {
        // 函数体的第一个 compound-stmt：作用域已在 FunDecl 中 enter 过
        context.pre_enter_scope = false;
        need_enter = false;
    }
    if (need_enter)
        scope.enter();

    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list) {
        stmt->accept(*this);
        if (builder->get_insert_block()->is_terminated())
            break;
    }

    if (need_enter)
        scope.exit();
    return nullptr;
}

Value* CminusfBuilder::visit(ASTExpressionStmt &node) {
    if (node.expression != nullptr) {
        return node.expression->accept(*this);
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSelectionStmt &node) {
    auto *ret_val = node.expression->accept(*this);
    auto *trueBB = BasicBlock::create(module.get(), "", context.func);
    BasicBlock *falseBB{};
    auto *contBB = BasicBlock::create(module.get(), "", context.func);
    Value *cond_val = nullptr;
    if (ret_val->get_type()->is_integer_type()) {
        cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
    } else {
        cond_val = builder->create_fcmp_ne(ret_val, CONST_FP(0.));
    }

    if (node.else_statement == nullptr) {
        builder->create_cond_br(cond_val, trueBB, contBB);
        builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);
        if (!builder->get_insert_block()->is_terminated()) {
            builder->create_br(contBB);
        }
    } else {
        falseBB = BasicBlock::create(module.get(), "", context.func);
        builder->create_cond_br(cond_val, trueBB, falseBB);
        builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);
        if (!builder->get_insert_block()->is_terminated()) {
            builder->create_br(contBB);
        }
        builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        if (!builder->get_insert_block()->is_terminated()) {
            builder->create_br(contBB);
        }
    }

    builder->set_insert_point(contBB);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTIterationStmt &node) {
    auto *func = context.func;
    auto *condBB = BasicBlock::create(module.get(), "", func);
    auto *bodyBB = BasicBlock::create(module.get(), "", func);
    auto *afterBB = BasicBlock::create(module.get(), "", func);

    // 先跳到条件块
    builder->create_br(condBB);

    // condBB
    builder->set_insert_point(condBB);
    auto *cond_val_any = node.expression->accept(*this);
    Value *cond_i1 = nullptr;
    if (cond_val_any->get_type()->is_integer_type()) {
        cond_i1 = builder->create_icmp_ne(cond_val_any, CONST_INT(0));
    } else {
        cond_i1 = builder->create_fcmp_ne(cond_val_any, CONST_FP(0.));
    }
    builder->create_cond_br(cond_i1, bodyBB, afterBB);

    // bodyBB
    builder->set_insert_point(bodyBB);
    node.statement->accept(*this);
    if (!builder->get_insert_block()->is_terminated()) {
        builder->create_br(condBB);
    }

    // afterBB
    builder->set_insert_point(afterBB);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTReturnStmt &node) {
    if (node.expression == nullptr) {
        builder->create_void_ret();
    } else {
        auto *fun_ret_type =
            context.func->get_function_type()->get_return_type();
        auto *ret_val = node.expression->accept(*this);
        if (fun_ret_type != ret_val->get_type()) {
            if (fun_ret_type->is_integer_type() &&
                ret_val->get_type()->is_float_type()) {
                ret_val = builder->create_fptosi(ret_val, INT32_T);
            } else if (fun_ret_type->is_float_type() &&
                       ret_val->get_type()->is_integer_type()) {
                ret_val = builder->create_sitofp(ret_val, FLOAT_T);
            }
        }

        builder->create_ret(ret_val);
    }

    return nullptr;
}

Value* CminusfBuilder::visit(ASTVar &node) {
    Value *baseAddr = this->scope.find(node.id);
    Type *alloctype = nullptr;

    if (baseAddr->is<AllocaInst>()) {
        alloctype = baseAddr->as<AllocaInst>()->get_alloca_type();
    } else {
        alloctype = baseAddr->as<GlobalVariable>()
                        ->get_type()
                        ->get_pointer_element_type();
    }

    if (node.expression) {
        bool original_require_lvalue = context.require_lvalue;
        context.require_lvalue = false;
        auto idx = node.expression->accept(*this);
        context.require_lvalue = original_require_lvalue;

        if (idx->get_type()->is_float_type()) {
            idx = builder->create_fptosi(idx, INT32_T);
        } else if (idx->get_type()->is_int1_type()) {
            idx = builder->create_zext(idx, INT32_T);
        }
        auto right_bb = BasicBlock::create(module.get(), "", context.func);
        auto wrong_bb = BasicBlock::create(module.get(), "", context.func);

        auto cond_neg = builder->create_icmp_ge(idx, CONST_INT(0));
        builder->create_cond_br(cond_neg, right_bb, wrong_bb);

        auto wrong = scope.find("neg_idx_except");
        builder->set_insert_point(wrong_bb);
        builder->create_call(wrong, {});
        builder->create_br(right_bb);
        builder->set_insert_point(right_bb);

        if (context.require_lvalue) {
            if (alloctype->is_pointer_type()) {
                baseAddr = builder->create_load(baseAddr);
                baseAddr = builder->create_gep(baseAddr, {idx});
            } else if (alloctype->is_array_type()) {
                baseAddr = builder->create_gep(baseAddr, {CONST_INT(0), idx});
            }
            context.require_lvalue = false;
            return baseAddr;
        } else {
            if (alloctype->is_pointer_type()) {
                baseAddr = builder->create_load(baseAddr);
                baseAddr = builder->create_gep(baseAddr, {idx});
            } else if (alloctype->is_array_type()) {
                baseAddr = builder->create_gep(baseAddr, {CONST_INT(0), idx});
            }
            baseAddr = builder->create_load(baseAddr);
            return baseAddr;
        }
    } else {
        if (context.require_lvalue) {
            context.require_lvalue = false;
            return baseAddr;
            // return builder->create_gep(baseAddr, {CONST_INT(0)});
        } else {
            if (alloctype->is_array_type()) {
                return builder->create_gep(baseAddr,
                                           {CONST_INT(0), CONST_INT(0)});
            } else {
                return builder->create_load(baseAddr);
            }
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAssignExpression &node) {
    auto *expr_result = node.expression->accept(*this);
    context.require_lvalue = true;
    auto *var_addr = node.var->accept(*this);
    if (var_addr->get_type()->get_pointer_element_type() !=
        expr_result->get_type()) {
        if (expr_result->get_type() == INT32_T) {
            expr_result = builder->create_sitofp(expr_result, FLOAT_T);
        } else {
            expr_result = builder->create_fptosi(expr_result, INT32_T);
        }
    }
    builder->create_store(expr_result, var_addr);
    return expr_result;
}

Value* CminusfBuilder::visit(ASTSimpleExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAdditiveExpression &node) {
    if (node.additive_expression == nullptr) {
        return node.term->accept(*this);
    }

    auto *l_val = node.additive_expression->accept(*this);
    auto *r_val = node.term->accept(*this);
    bool is_int = promote(&*builder, &l_val, &r_val);
    Value *ret_val = nullptr;
    switch (node.op) {
    case OP_PLUS:
        if (is_int) {
            ret_val = builder->create_iadd(l_val, r_val);
        } else {
            ret_val = builder->create_fadd(l_val, r_val);
        }
        break;
    case OP_MINUS:
        if (is_int) {
            ret_val = builder->create_isub(l_val, r_val);
        } else {
            ret_val = builder->create_fsub(l_val, r_val);
        }
        break;
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTTerm &node) {
    if (node.term == nullptr) {
        return node.factor->accept(*this);
    }

    auto *l_val = node.term->accept(*this);
    auto *r_val = node.factor->accept(*this);
    bool is_int = promote(&*builder, &l_val, &r_val);

    Value *ret_val = nullptr;
    switch (node.op) {
    case OP_MUL:
        if (is_int) {
            ret_val = builder->create_imul(l_val, r_val);
        } else {
            ret_val = builder->create_fmul(l_val, r_val);
        }
        break;
    case OP_DIV:
        if (is_int) {
            ret_val = builder->create_isdiv(l_val, r_val);
        } else {
            ret_val = builder->create_fdiv(l_val, r_val);
        }
        break;
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTCall &node) {
    auto *func = dynamic_cast<Function *>(scope.find(node.id));
    std::vector<Value *> args;
    auto param_type = func->get_function_type()->param_begin();
    for (auto &arg : node.args) {
        auto *arg_val = arg->accept(*this);
        if (!arg_val->get_type()->is_pointer_type() &&
            *param_type != arg_val->get_type()) {
            if (arg_val->get_type()->is_integer_type()) {
                arg_val = builder->create_sitofp(arg_val, FLOAT_T);
            } else {
                arg_val = builder->create_fptosi(arg_val, INT32_T);
            }
        }
        args.push_back(arg_val);
        param_type++;
    }

    return builder->create_call(static_cast<Function *>(func), args);
}
