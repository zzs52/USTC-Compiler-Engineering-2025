#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "Function.hpp"
#include "IRBuilder.hpp"
#include "Module.hpp"
#include "Type.hpp"

#include <iostream>
#include <memory>

#ifdef DEBUG
#define DEBUG_OUTPUT std::cout << __LINE__ << std::endl;  // 输出行号的简单示例
#else
#define DEBUG_OUTPUT
#endif

#define CONST_INT(num) ConstantInt::get(num, module)

#define CONST_FP(num) ConstantFP::get(num, module)  // 得到常数值的表示，方便后面多次用到

int main() {
    auto module = new Module();
    auto builder = new IRBuilder(nullptr, module);

    // main 函数
    Type *Int32Type = module->get_int32_type();
    auto *mainFun = Function::create(FunctionType::get(Int32Type, {}),
                                     "main", module);
    // 创建基本块
    auto *bb = BasicBlock::create(module, "entry", mainFun);
    builder->set_insert_point(bb);
    // 数组 a
    auto *arrayType = ArrayType::get(Int32Type, 10);
    auto *aAlloca = builder->create_alloca(arrayType);
    // 指向 a[0] 的指针
    auto *a0GEP = builder->create_gep(aAlloca, {CONST_INT(0), CONST_INT(0)});
    builder->create_store(CONST_INT(10), a0GEP);
    // 指向 a[1] 的指针
    auto *a1GEP = builder->create_gep(aAlloca, {CONST_INT(0), CONST_INT(1)});
    auto *a0 = builder->create_load(a0GEP);
    auto *mul = builder->create_imul(a0, CONST_INT(2));
    builder->create_store(mul, a1GEP);
    auto *a1 = builder->create_load(a1GEP);
    builder->create_ret(a1);
    std::cout << module->print();
    delete module;
    return 0;
}
