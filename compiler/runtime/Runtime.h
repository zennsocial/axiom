#pragma once

#include <QtCore/QMutex>
#include <mutex>

#include "Jit.h"
#include "ValueOperator.h"
#include "RootSurface.h"
#include "../codegen/MaximContext.h"

namespace MaximCodegen {
    class MaximContext;
};

namespace MaximRuntime {

    struct QueuedEvent {
        int64_t deltaFrames;
        MidiEventValue event;
    };

    class Runtime {
    public:
        static constexpr size_t eventQueueSize = 256;

        Runtime();

        MaximCodegen::MaximContext *ctx() { return &_context; }

        Jit &jit() { return _jit; }

        ValueOperator &op() { return _op; }

        RootSurface *mainSurface() { return _mainSurface.get(); }

        void compile();

        void generate();

        void queueEvent(const QueuedEvent &event);

        void clearEvents();

        void fillBuffer(float **buffer, size_t size);

        void fillPartialBuffer(float **buffer, size_t length, const MidiValue &event);

    private:
        std::mutex _mutex;
        Jit _jit;
        MaximCodegen::MaximContext _context;
        ValueOperator _op;
        std::unique_ptr<RootSurface> _mainSurface;

        QueuedEvent _queuedEvents[eventQueueSize];

        bool _isDeployed = false;
        Jit::ModuleKey _deployKey;

        void (*_generateFuncPtr)() = nullptr;
        void (*_cleanupFuncPtr)() = nullptr;

        llvm::Function *createForwardFunc(llvm::Module *module, std::string name, llvm::Value *ctx,
                                          MaximCodegen::ModuleClassMethod *method);
    };

}
