#include "model_openvr.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace lumaframevr::application {
struct RuntimeControllerModels::Impl {
    // std::atomic<std::shared_ptr<T>> requires a trivially copyable type, which a
    // shared_ptr is not, and the C++20 specialization for it only exists from GCC
    // 12 onwards. A small lock gives the same guarantee: a reader always sees one
    // complete, immutable snapshot.
    mutable std::mutex snapshot_lock;
    std::shared_ptr<const RuntimeModelSnapshot> snapshot;
    std::jthread worker;
    void publish(RuntimeModelSnapshot result) {
        std::lock_guard lock(snapshot_lock);
        snapshot = std::make_shared<RuntimeModelSnapshot>(std::move(result));
    }
    explicit Impl(std::string path) {
        publish(RuntimeModelSnapshot{});
        worker = std::jthread([this, path = std::move(path)](std::stop_token stop) {
            try {
                auto session = model_detail::acquire_session(path);
                std::array<model_detail::HandWork, 2> hands;
                std::mutex wait_mutex;
                std::condition_variable_any wake;
                while(!stop.stop_requested()) {
                    RuntimeModelSnapshot result;
                    {
                        std::lock_guard lock(session->api_mutex);
                        model_detail::advance_hand(session, hands[0], ETrackedControllerRole_TrackedControllerRole_LeftHand);
                        model_detail::advance_hand(session, hands[1], ETrackedControllerRole_TrackedControllerRole_RightHand);
                        result.hands = {hands[0].snapshot, hands[1].snapshot};
                    }
                    publish(std::move(result));
                    std::unique_lock lock(wait_mutex);
                    wake.wait_for(lock, stop, std::chrono::milliseconds(20), [] { return false; });
                }
            } catch(const std::exception& error) {
                RuntimeModelSnapshot result;
                for(auto& hand : result.hands) hand = {ModelLoadStatus::Unavailable, error.what(), {}};
                publish(std::move(result));
            }
        });
    }
    ~Impl() {
        // Stop the worker and wait for it before the OpenVR session is torn down.
        // The worker can be inside a blocking OpenVR call, so the join must happen
        // here, while the session is still alive, not after it is destroyed.
        worker.request_stop();
        if(worker.joinable())
            worker.join();
    }
};
RuntimeControllerModels::RuntimeControllerModels(std::string path) : impl_(std::make_unique<Impl>(std::move(path))) {}
RuntimeControllerModels::~RuntimeControllerModels() = default;
void RuntimeControllerModels::shutdown() {
    if(!impl_)
        return;
    impl_->worker.request_stop();
    if(impl_->worker.joinable())
        impl_->worker.join();
}
RuntimeModelSnapshot RuntimeControllerModels::poll() const {
    std::lock_guard lock(impl_->snapshot_lock);
    return *impl_->snapshot;
}
}  // namespace lumaframevr::application
