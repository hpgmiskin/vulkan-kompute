
#include <iostream>
#include <memory>
#include <vector>

#include "kompute/Kompute.hpp"

int main()
{
#if KOMPUTE_ENABLE_SPDLOG
    spdlog::set_level(
      static_cast<spdlog::level::level_enum>(SPDLOG_ACTIVE_LEVEL));
#endif

    kp::Manager mgr;

    auto tensorInA = mgr.tensor({ 2.0, 4.0, 6.0 });
    auto tensorInB = mgr.tensor({ 0.0, 1.0, 2.0 });
    auto tensorOut = mgr.tensor({ 0.0, 0.0, 0.0 });

    std::string shader(R"(
        // The version to use 
        #version 450

        // The execution structure
        layout (local_size_x = 1) in;

        // The buffers are provided via the tensors
        layout(binding = 0) buffer bufA { float a[]; };
        layout(binding = 1) buffer bufB { float b[]; };
        layout(binding = 2) buffer bufOut { float o[]; };

        void main() {
            uint index = gl_GlobalInvocationID.x;

            o[index] = a[index] * b[index];
        }
      )");

    mgr.evalOpDefault<kp::OpAlgoBase>(
            { tensorInA, tensorInB, tensorOut },
            kp::Shader::compile_source(shader));

    mgr.evalOpDefault<kp::OpTensorSyncLocal>({tensorOut});

    // prints "Output {  0  4  12  }"
    std::cout<< "Output: {  ";
    for (const float& elem : tensorOut->data()) {
      std::cout << elem << "  ";
    }
    std::cout << "}" << std::endl;
}

