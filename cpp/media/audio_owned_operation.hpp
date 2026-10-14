#pragma once
#include <pulse/pulseaudio.h>
#include <memory>

namespace media {
struct OperationDeleter {
    bool cancel;
    void operator()(pa_operation* operation) const noexcept {
        if(cancel)
            pa_operation_cancel(operation);
        pa_operation_unref(operation);
    }
};
using OwnedOperation = std::unique_ptr<pa_operation, OperationDeleter>;

inline void clear_operation(pa_operation*& operation, bool cancel) {
    OwnedOperation owner(operation, OperationDeleter{cancel});
    owner.reset();
    operation = nullptr;
}
}  // namespace media
