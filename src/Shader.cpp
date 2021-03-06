
#if !defined(KOMPUTE_DISABLE_SHADER_UTILS) || !KOMPUTE_DISABLE_SHADER_UTILS
#include "kompute/Shader.hpp"

namespace kp {

std::vector<uint32_t>
Shader::compile_sources(const std::vector<std::string>& sources,
                                   const std::vector<std::string>& files,
                                   const std::string& entryPoint,
                                   std::vector<std::pair<std::string,std::string>> definitions,
                                   const TBuiltInResource& resources) {

    // Initialize glslang library.
    glslang::InitializeProcess();

    // Currently we don't support other shader types nor plan to
    const EShLanguage language = EShLangCompute;
    glslang::TShader shader(language);

    std::vector<const char*> filesCStr(files.size()), sourcesCStr(sources.size());
    for (size_t i = 0; i < sources.size(); i++) sourcesCStr[i] = sources[i].c_str();

    if (files.size() > 1) {
        assert(files.size() == sources.size());
        for (size_t i = 0; i < files.size(); i++) filesCStr[i] = files[i].c_str();
        shader.setStringsWithLengthsAndNames(sourcesCStr.data(), nullptr, filesCStr.data(), filesCStr.size());
    }
    else {
        filesCStr = {""};
        shader.setStringsWithLengthsAndNames(sourcesCStr.data(), nullptr, filesCStr.data(), sourcesCStr.size());
    }

    shader.setEntryPoint(entryPoint.c_str());
    shader.setSourceEntryPoint(entryPoint.c_str());

    std::string info_log = "";
    const EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);
    if (!shader.parse(&resources, 100, false, messages))
    {
        info_log = std::string(shader.getInfoLog()) + "\n" + std::string(shader.getInfoDebugLog());
        KP_LOG_ERROR("Kompute Shader Error: {}", info_log);
        throw std::runtime_error(info_log);
    }

    // Add shader to new program object.
    glslang::TProgram program;
    program.addShader(&shader);
    // Link program.
    if (!program.link(messages))
    {
        info_log = std::string(program.getInfoLog()) + "\n" + std::string(program.getInfoDebugLog());
        KP_LOG_ERROR("Kompute Shader Error: {}", info_log);
        throw std::runtime_error(info_log);
    }

    // Save any info log that was generated.
    if (shader.getInfoLog())
    {
        info_log += std::string(shader.getInfoLog()) + "\n" + std::string(shader.getInfoDebugLog()) + "\n";
        KP_LOG_INFO("Kompute Shader Information: {}", info_log);
    }

    glslang::TIntermediate *intermediate = program.getIntermediate(language);
    // Translate to SPIRV.
    if (!intermediate)
    {
        info_log += "Failed to get shared intermediate code.\n";
        KP_LOG_ERROR("Kompute Shader Error: {}", info_log);
        throw std::runtime_error(info_log);
    }

    spv::SpvBuildLogger logger;
    std::vector<std::uint32_t> spirv;
    glslang::GlslangToSpv(*intermediate, spirv, &logger);

    if (shader.getInfoLog())
    {
        info_log += logger.getAllMessages() + "\n";
        KP_LOG_DEBUG("Kompute Shader all result messages: {}", info_log);
    }

    // Shutdown glslang library.
    glslang::FinalizeProcess();

    return spirv;
}

std::vector<uint32_t>
Shader::compile_source(const std::string& source,
        const std::string& entryPoint,
        std::vector<std::pair<std::string,std::string>> definitions,
        const TBuiltInResource& resource) {
    return compile_sources({source}, std::vector<std::string>({}), entryPoint, definitions, resource);
}

}
#endif // DKOMPUTE_DISABLE_SHADER_UTILS
