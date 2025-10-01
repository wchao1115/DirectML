#pragma once

class CommandLineArgs;

class Executor
{
public:
    Executor(Model& model, std::shared_ptr<Device> device, const CommandLineArgs& args, IDxDispatchLogger* logger);

    uint32_t GetCommandCount();
    void RunCommand(UINT32 id);
    void Run();
    void operator()(const Model::DispatchCommand& command);
    void operator()(const Model::PrintCommand& command);
    void operator()(const Model::WriteFileCommand& command);
    void operator()(const Model::ResolveGpuTimeCommand& command);

private:
    Dispatchable::Bindings ResolveBindings(const Model::Bindings& modelBindings);

private:
    Model& m_model;
    std::shared_ptr<Device> m_device;
    const CommandLineArgs& m_commandLineArgs;
    std::unordered_map<std::string, std::unique_ptr<Dispatchable>> m_dispatchables;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> m_resources;
    // Resource descriptors synthesized at runtime (when 'resources' section omitted or missing entries).
    std::vector<Model::ResourceDesc> m_inferredResourceDescs;
    Dispatchable::DeferredBindings m_deferredBinding;
    Microsoft::WRL::ComPtr<IDxDispatchLogger> m_logger;
    UINT32 m_nextId = 0;
};