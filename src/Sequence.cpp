
#include "kompute/Sequence.hpp"

namespace kp {

Sequence::Sequence()
{
    KP_LOG_DEBUG("Kompute Sequence base constructor");
    this->mIsInit = false;
}

Sequence::Sequence(std::shared_ptr<vk::PhysicalDevice> physicalDevice,
                   std::shared_ptr<vk::Device> device,
                   std::shared_ptr<vk::Queue> computeQueue,
                   uint32_t queueIndex)
{
    KP_LOG_DEBUG("Kompute Sequence Constructor with existing device & queue");

    this->mPhysicalDevice = physicalDevice;
    this->mDevice = device;
    this->mComputeQueue = computeQueue;
    this->mQueueIndex = queueIndex;
    this->mIsInit = false;
}

Sequence::~Sequence()
{
    KP_LOG_DEBUG("Kompute Sequence Destructor started");

    if (!this->mIsInit) {
        KP_LOG_INFO("Kompute Sequence destructor called but sequence is not "
                    "initialized so no need to removing GPU resources.");
        return;
    } else {
        this->freeMemoryDestroyGPUResources();
    }
}

void
Sequence::init()
{
    this->createCommandPool();
    this->createCommandBuffer();
    this->mIsInit = true;
}

bool
Sequence::begin()
{
    KP_LOG_DEBUG("Kompute sequence called BEGIN");

    if (this->isRecording()) {
        KP_LOG_WARN("Kompute Sequence begin called when  already recording");
        return false;
    }

    if (this->isRunning()) {
        KP_LOG_WARN(
          "Kompute Sequence begin called when sequence still running");
        return false;
    }

    if (!this->mCommandPool) {
        throw std::runtime_error("Kompute Sequence command pool is null");
    }

    if (this->mOperations.size()) {
        KP_LOG_INFO("Kompute Sequence clearing previous operations");
        this->mOperations.clear();
    }

    if (!this->mRecording) {
        KP_LOG_INFO("Kompute Sequence command recording BEGIN");
        this->mCommandBuffer->begin(vk::CommandBufferBeginInfo());
        this->mRecording = true;
    } else {
        KP_LOG_WARN("Kompute Sequence attempted to start command recording "
                    "but recording already started");
    }
    return true;
}

bool
Sequence::end()
{
    KP_LOG_DEBUG("Kompute Sequence calling END");

    if (!this->isRecording()) {
        KP_LOG_WARN("Kompute Sequence end called when not recording");
        return false;
    }

    if (!this->mCommandPool) {
        throw std::runtime_error("Kompute Sequence command pool is null");
    }

    if (this->mRecording) {
        KP_LOG_INFO("Kompute Sequence command recording END");
        this->mCommandBuffer->end();
        this->mRecording = false;
    } else {
        KP_LOG_WARN("Kompute Sequence attempted to end command recording but "
                    "recording not started");
    }
    return true;
}

bool
Sequence::eval()
{
    KP_LOG_DEBUG("Kompute sequence EVAL BEGIN");

    bool evalResult = this->evalAsync();
    if (!evalResult) {
        KP_LOG_DEBUG("Kompute sequence EVAL FAILURE");
        return false;
    }

    evalResult = this->evalAwait();

    KP_LOG_DEBUG("Kompute sequence EVAL SUCCESS");

    return evalResult;
}

bool
Sequence::evalAsync()
{
    if (this->isRecording()) {
        KP_LOG_WARN("Kompute Sequence evalAsync called when still recording");
        return false;
    }
    if (this->mIsRunning) {
        KP_LOG_WARN("Kompute Sequence evalAsync called when an eval async was "
                    "called without successful wait");
        return false;
    }

    this->mIsRunning = true;

    for (size_t i = 0; i < this->mOperations.size(); i++) {
        this->mOperations[i]->preEval();
    }

    vk::SubmitInfo submitInfo(
      0, nullptr, nullptr, 1, this->mCommandBuffer.get());

    this->mFence = this->mDevice->createFence(vk::FenceCreateInfo());

    KP_LOG_DEBUG(
      "Kompute sequence submitting command buffer into compute queue");

    this->mComputeQueue->submit(1, &submitInfo, this->mFence);

    return true;
}

bool
Sequence::evalAwait(uint64_t waitFor)
{
    if (!this->mIsRunning) {
        KP_LOG_WARN("Kompute Sequence evalAwait called without existing eval");
        return false;
    }

    vk::Result result =
      this->mDevice->waitForFences(1, &this->mFence, VK_TRUE, waitFor);
    this->mDevice->destroy(
      this->mFence, (vk::Optional<const vk::AllocationCallbacks>)nullptr);

    this->mIsRunning = false;

    if (result == vk::Result::eTimeout) {
        KP_LOG_WARN("Kompute Sequence evalAwait timed out");
        return false;
    }

    for (size_t i = 0; i < this->mOperations.size(); i++) {
        this->mOperations[i]->postEval();
    }

    return true;
}

bool
Sequence::isRunning()
{
    return this->mIsRunning;
}

bool
Sequence::isRecording()
{
    return this->mRecording;
}

bool
Sequence::isInit()
{
    return this->mIsInit;
}

void
Sequence::freeMemoryDestroyGPUResources()
{
    KP_LOG_DEBUG("Kompute Sequence freeMemoryDestroyGPUResources called");

    if (!this->mIsInit) {
        KP_LOG_ERROR("Kompute Sequence freeMemoryDestroyGPUResources called "
                     "but Sequence is not initialized so there's no relevant "
                     "GPU resources.");
        return;
    }

    if (!this->mDevice) {
        KP_LOG_ERROR("Kompute Sequence freeMemoryDestroyGPUResources called "
                     "with null Device pointer");
        this->mIsInit = false;
        return;
    }

    if (this->mFreeCommandBuffer) {
        KP_LOG_INFO("Freeing CommandBuffer");
        if (!this->mCommandBuffer) {
            KP_LOG_ERROR(
              "Kompute Sequence freeMemoryDestroyGPUResources called with null "
              "CommandPool pointer");
            this->mIsInit = false;
            return;
        }
        this->mDevice->freeCommandBuffers(
          *this->mCommandPool, 1, this->mCommandBuffer.get());
        KP_LOG_DEBUG("Kompute Sequence Freed CommandBuffer");
    }

    if (this->mFreeCommandPool) {
        KP_LOG_INFO("Destroying CommandPool");
        if (this->mCommandPool == nullptr) {
            KP_LOG_ERROR(
              "Kompute Sequence freeMemoryDestroyGPUResources called with null "
              "CommandPool pointer");
            this->mIsInit = false;
            return;
        }
        this->mDevice->destroy(
          *this->mCommandPool,
          (vk::Optional<const vk::AllocationCallbacks>)nullptr);
        KP_LOG_DEBUG("Kompute Sequence Destroyed CommandPool");
    }

    if (this->mOperations.size()) {
        KP_LOG_INFO("Kompute Sequence clearing operations buffer");
        this->mOperations.clear();
    }

    this->mIsInit = false;
}

void
Sequence::createCommandPool()
{
    KP_LOG_DEBUG("Kompute Sequence creating command pool");

    if (!this->mDevice) {
        throw std::runtime_error("Kompute Sequence device is null");
    }
    if (this->mQueueIndex < 0) {
        throw std::runtime_error("Kompute Sequence queue index not provided");
    }

    this->mFreeCommandPool = true;

    vk::CommandPoolCreateInfo commandPoolInfo(vk::CommandPoolCreateFlags(),
                                              this->mQueueIndex);
    this->mCommandPool = std::make_shared<vk::CommandPool>();
    this->mDevice->createCommandPool(
      &commandPoolInfo, nullptr, this->mCommandPool.get());
    KP_LOG_DEBUG("Kompute Sequence Command Pool Created");
}

void
Sequence::createCommandBuffer()
{
    KP_LOG_DEBUG("Kompute Sequence creating command buffer");
    if (!this->mDevice) {
        throw std::runtime_error("Kompute Sequence device is null");
    }
    if (!this->mCommandPool) {
        throw std::runtime_error("Kompute Sequence command pool is null");
    }

    this->mFreeCommandBuffer = true;

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
      *this->mCommandPool, vk::CommandBufferLevel::ePrimary, 1);

    this->mCommandBuffer = std::make_shared<vk::CommandBuffer>();
    this->mDevice->allocateCommandBuffers(&commandBufferAllocateInfo,
                                          this->mCommandBuffer.get());
    KP_LOG_DEBUG("Kompute Sequence Command Buffer Created");
}

}
