
#include "gtest/gtest.h"

#include "kompute/Kompute.hpp"

#include "kompute_test/shaders/shadertest_logistic_regression.hpp"

TEST(TestLogisticRegression, TestMainLogisticRegression)
{

    uint32_t ITERATIONS = 100;
    float learningRate = 0.1;

    std::shared_ptr<kp::Tensor> xI{ new kp::Tensor({ 0, 1, 1, 1, 1 }) };
    std::shared_ptr<kp::Tensor> xJ{ new kp::Tensor({ 0, 0, 0, 1, 1 }) };

    std::shared_ptr<kp::Tensor> y{ new kp::Tensor({ 0, 0, 0, 1, 1 }) };

    std::shared_ptr<kp::Tensor> wIn{ new kp::Tensor({ 0.001, 0.001 }) };
    std::shared_ptr<kp::Tensor> wOutI{ new kp::Tensor({ 0, 0, 0, 0, 0 }) };
    std::shared_ptr<kp::Tensor> wOutJ{ new kp::Tensor({ 0, 0, 0, 0, 0 }) };

    std::shared_ptr<kp::Tensor> bIn{ new kp::Tensor({ 0 }) };
    std::shared_ptr<kp::Tensor> bOut{ new kp::Tensor({ 0, 0, 0, 0, 0 }) };

    std::shared_ptr<kp::Tensor> lOut{ new kp::Tensor({ 0, 0, 0, 0, 0 }) };

    std::vector<std::shared_ptr<kp::Tensor>> params = { xI,  xJ,    y,
                                                        wIn, wOutI, wOutJ,
                                                        bIn, bOut,  lOut };

    {
        kp::Manager mgr;

        mgr.rebuild(params);

        std::shared_ptr<kp::Sequence> sq = mgr.sequence();

        // Record op algo base
        sq->begin();

        sq->record<kp::OpTensorSyncDevice>({ wIn, bIn });

        sq->record<kp::OpAlgoBase>(
          params,
          std::vector<uint32_t>(
            (uint32_t*)kp::shader_data::shaders_glsl_logisticregression_comp_spv,
            (uint32_t*)(kp::shader_data::shaders_glsl_logisticregression_comp_spv +
              kp::shader_data::shaders_glsl_logisticregression_comp_spv_len)),
          kp::Workgroup(), kp::Constants({5.0}));

        sq->record<kp::OpTensorSyncLocal>({ wOutI, wOutJ, bOut, lOut });

        sq->end();

        // Iterate across all expected iterations
        for (size_t i = 0; i < ITERATIONS; i++) {

            sq->eval();

            for (size_t j = 0; j < bOut->size(); j++) {
                wIn->data()[0] -= learningRate * wOutI->data()[j];
                wIn->data()[1] -= learningRate * wOutJ->data()[j];
                bIn->data()[0] -= learningRate * bOut->data()[j];
            }
        }
    }

    // Based on the inputs the outputs should be at least:
    // * wi < 0.01
    // * wj > 1.0
    // * b < 0
    // TODO: Add EXPECT_DOUBLE_EQ instead
    EXPECT_LT(wIn->data()[0], 0.01);
    EXPECT_GT(wIn->data()[1], 1.0);
    EXPECT_LT(bIn->data()[0], 0.0);

    KP_LOG_WARN("Result wIn i: {}, wIn j: {}, bIn: {}",
                wIn->data()[0],
                wIn->data()[1],
                bIn->data()[0]);
}

TEST(TestLogisticRegression, TestMainLogisticRegressionManualCopy)
{

    uint32_t ITERATIONS = 100;
    float learningRate = 0.1;

    kp::Constants wInVec = { 0.001, 0.001 };
    std::vector<float> bInVec = { 0 };

    std::shared_ptr<kp::Tensor> xI{ new kp::Tensor({ 0, 1, 1, 1, 1 }) };
    std::shared_ptr<kp::Tensor> xJ{ new kp::Tensor({ 0, 0, 0, 1, 1 }) };

    std::shared_ptr<kp::Tensor> y{ new kp::Tensor({ 0, 0, 0, 1, 1 }) };

    std::shared_ptr<kp::Tensor> wIn{ new kp::Tensor(
      wInVec, kp::Tensor::TensorTypes::eHost) };
    std::shared_ptr<kp::Tensor> wOutI{ new kp::Tensor({ 0, 0, 0, 0, 0 }) };
    std::shared_ptr<kp::Tensor> wOutJ{ new kp::Tensor({ 0, 0, 0, 0, 0 }) };

    std::shared_ptr<kp::Tensor> bIn{ new kp::Tensor(
      bInVec, kp::Tensor::TensorTypes::eHost) };
    std::shared_ptr<kp::Tensor> bOut{ new kp::Tensor({ 0, 0, 0, 0, 0 }) };

    std::shared_ptr<kp::Tensor> lOut{ new kp::Tensor({ 0, 0, 0, 0, 0 }) };

    std::vector<std::shared_ptr<kp::Tensor>> params = { xI,  xJ,    y,
                                                        wIn, wOutI, wOutJ,
                                                        bIn, bOut,  lOut };

    {
        kp::Manager mgr;

        mgr.rebuild(params);

        std::shared_ptr<kp::Sequence> sq = mgr.sequence();

        // Record op algo base
        sq->begin();

        sq->record<kp::OpAlgoBase>(
          params,
          std::vector<uint32_t>(
            (uint32_t*)kp::shader_data::shaders_glsl_logisticregression_comp_spv,
            (uint32_t*)(kp::shader_data::shaders_glsl_logisticregression_comp_spv +
              kp::shader_data::shaders_glsl_logisticregression_comp_spv_len)),
          kp::Workgroup(), kp::Constants({5.0}));

        sq->record<kp::OpTensorSyncLocal>({ wOutI, wOutJ, bOut, lOut });

        sq->end();

        // Iterate across all expected iterations
        for (size_t i = 0; i < ITERATIONS; i++) {

            sq->eval();

            for (size_t j = 0; j < bOut->size(); j++) {
                wIn->data()[0] -= learningRate * wOutI->data()[j];
                wIn->data()[1] -= learningRate * wOutJ->data()[j];
                bIn->data()[0] -= learningRate * bOut->data()[j];
            }
            wIn->mapDataIntoHostMemory();
            bIn->mapDataIntoHostMemory();
        }
    }

    // Based on the inputs the outputs should be at least:
    // * wi < 0.01
    // * wj > 1.0
    // * b < 0
    // TODO: Add EXPECT_DOUBLE_EQ instead
    EXPECT_LT(wIn->data()[0], 0.01);
    EXPECT_GT(wIn->data()[1], 1.0);
    EXPECT_LT(bIn->data()[0], 0.0);

    KP_LOG_WARN("Result wIn i: {}, wIn j: {}, bIn: {}",
                wIn->data()[0],
                wIn->data()[1],
                bIn->data()[0]);
}
