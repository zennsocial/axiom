#include "LowBqFilterFunction.h"

#include "../MaximContext.h"
#include "../ComposableModuleClassMethod.h"
#include "../Num.h"

using namespace MaximCodegen;

LowBqFilterFunction::LowBqFilterFunction(MaximCodegen::MaximContext *ctx, llvm::Module *module,
                                         MaximCodegen::Function *biquadFilterFunction)
     : Function(ctx, module, "lowBqFilter",
                ctx->numType(),
                {Parameter(ctx->numType(), false, false), // input
                 Parameter(ctx->numType(), false, false), // frequency
                 Parameter(ctx->numType(), false, false)  // Q
                }, nullptr), biquadFilterFunction(biquadFilterFunction) {
}

std::unique_ptr<LowBqFilterFunction> LowBqFilterFunction::create(MaximCodegen::MaximContext *ctx,
                                                                 llvm::Module *module, Function *biquadFilterFunction) {
    return std::make_unique<LowBqFilterFunction>(ctx, module, biquadFilterFunction);
}

std::unique_ptr<Value> LowBqFilterFunction::generate(MaximCodegen::ComposableModuleClassMethod *method,
                                                     const std::vector<std::unique_ptr<MaximCodegen::Value>> &params,
                                                     std::unique_ptr<MaximCodegen::VarArg> vararg) {
    auto floatTy = llvm::Type::getFloatTy(ctx()->llvm());
    auto tanFunc = method->moduleClass()->module()->getFunction("tanf");
    if (!tanFunc) {
        tanFunc = llvm::Function::Create(
            llvm::FunctionType::get(floatTy, std::vector<llvm::Type *>{floatTy}, false),
            llvm::Function::ExternalLinkage,
            "tanf", method->moduleClass()->module()
        );
    }

    auto inputNum = dynamic_cast<Num *>(params[0].get());
    auto freqNum = dynamic_cast<Num *>(params[1].get());
    auto qNum = dynamic_cast<Num *>(params[2].get());
    assert(inputNum && freqNum && qNum);

    auto &b = method->builder();
    auto cachedFreqPtr = method->getEntryPointer(method->moduleClass()->addEntry(ctx()->floatVecTy()), "cachedfreq.ptr");
    auto cachedQPtr = method->getEntryPointer(method->moduleClass()->addEntry(ctx()->floatVecTy()), "cachedq.ptr");
    auto a0Ptr = method->getEntryPointer(method->moduleClass()->addEntry(ctx()->floatVecTy()), "a0.ptr");
    auto a1Ptr = method->getEntryPointer(method->moduleClass()->addEntry(ctx()->floatVecTy()), "a1.ptr");
    auto a2Ptr = method->getEntryPointer(method->moduleClass()->addEntry(ctx()->floatVecTy()), "a2.ptr");
    auto b1Ptr = method->getEntryPointer(method->moduleClass()->addEntry(ctx()->floatVecTy()), "b1.ptr");
    auto b2Ptr = method->getEntryPointer(method->moduleClass()->addEntry(ctx()->floatVecTy()), "b2.ptr");

    auto freqVec = freqNum->vec(b);
    auto qVec = qNum->vec(b);

    auto freqChanged = b.CreateFCmpONE(freqVec, b.CreateLoad(cachedFreqPtr), "freqchanged");
    auto qChanged = b.CreateFCmpONE(qVec, b.CreateLoad(cachedQPtr), "qchanged");
    auto needsRegenVec = b.CreateOr(freqChanged, qChanged, "needsregen");
    auto needsRegen = b.CreateOr(b.CreateExtractElement(needsRegenVec, (uint64_t) 0), b.CreateExtractElement(needsRegenVec, 1));

    auto regenBlock = llvm::BasicBlock::Create(ctx()->llvm(), "regen", method->get(method->moduleClass()->module()));
    auto continueBlock = llvm::BasicBlock::Create(ctx()->llvm(), "continue", method->get(method->moduleClass()->module()));
    b.CreateCondBr(needsRegen, regenBlock, continueBlock);
    b.SetInsertPoint(regenBlock);

    // calculate K value = tan(PI * freq / sampleRate)
    auto kParam = b.CreateFDiv(
        b.CreateFMul(
            ctx()->constFloatVec(M_PI),
            freqVec
        ),
        ctx()->constFloatVec(ctx()->sampleRate)
    );
    auto leftK = b.CreateCall(tanFunc, { b.CreateExtractElement(kParam, (uint64_t) 0) }, "leftk");
    auto rightK = b.CreateCall(tanFunc, { b.CreateExtractElement(kParam, (uint64_t) 1) }, "rightk");
    llvm::Value *kValue = llvm::UndefValue::get(ctx()->floatVecTy());
    kValue = b.CreateInsertElement(kValue, leftK, (uint64_t) 0, "kvalue");
    kValue = b.CreateInsertElement(kValue, rightK, (uint64_t) 1, "kvalue");
    auto kSquared = b.CreateFMul(kValue, kValue);

    // norm = 1 / (1 + K / q + K * K)
    auto norm = b.CreateFDiv(
        ctx()->constFloatVec(1),
        b.CreateFAdd(
            b.CreateFAdd(
                ctx()->constFloatVec(1),
                b.CreateFDiv(
                    kValue,
                    qVec
                )
            ),
            kSquared
        )
    );

    // a0 = K * K * norm
    auto a0 = b.CreateFMul(kSquared, norm);
    b.CreateStore(a0, a0Ptr);

    // a1 = 2 * a0
    auto a1 = b.CreateFMul(ctx()->constFloatVec(2), a0);
    b.CreateStore(a1, a1Ptr);

    // a2 = a0
    auto a2 = a0;
    b.CreateStore(a2, a2Ptr);

    // b1 = 2 * (K * K - 1) * norm
    auto b1 = b.CreateFMul(
        b.CreateFMul(
            ctx()->constFloatVec(2),
            b.CreateFSub(kSquared, ctx()->constFloatVec(1))
        ),
        norm
    );
    b.CreateStore(b1, b1Ptr);

    // b2 = (1 - K / q + K * K) * norm
    auto b2 = b.CreateFMul(
        b.CreateFAdd(
            b.CreateFSub(
                ctx()->constFloatVec(1),
                b.CreateFDiv(kValue, qVec)
            ),
            kSquared
        ),
        norm
    );
    b.CreateStore(b2, b2Ptr);
    b.CreateBr(continueBlock);
    b.SetInsertPoint(continueBlock);

    std::vector<std::unique_ptr<Value>> biquadParams;
    biquadParams.push_back(Num::create(ctx(), inputNum->get(), b, method->allocaBuilder()));

    auto a0Num = Num::create(ctx(), method->allocaBuilder());
    a0Num->setVec(b, b.CreateLoad(a0Ptr));
    biquadParams.push_back(std::move(a0Num));

    auto a1Num = Num::create(ctx(), method->allocaBuilder());
    a1Num->setVec(b, b.CreateLoad(a1Ptr));
    biquadParams.push_back(std::move(a1Num));

    auto a2Num = Num::create(ctx(), method->allocaBuilder());
    a2Num->setVec(b, b.CreateLoad(a2Ptr));
    biquadParams.push_back(std::move(a2Num));

    auto b1Num = Num::create(ctx(), method->allocaBuilder());
    b1Num->setVec(b, b.CreateLoad(b1Ptr));
    biquadParams.push_back(std::move(b1Num));

    auto b2Num = Num::create(ctx(), method->allocaBuilder());
    b2Num->setVec(b, b.CreateLoad(b2Ptr));
    biquadParams.push_back(std::move(b2Num));

    return biquadFilterFunction->call(method, std::move(biquadParams), SourcePos(-1, -1), SourcePos(-1, -1));
}
