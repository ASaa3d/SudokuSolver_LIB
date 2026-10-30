#ifndef ONNX_SESSION_H
#define ONNX_SESSION_H
#include <onnxruntime_cxx_api.h>


class onnx_session
{
    private:
        std::unique_ptr<Ort::Env> env = nullptr;  
        std::unique_ptr<Ort::Session> session = nullptr;
        std::string model_path;
        std::string in_name;
        std::string out_name;
        std::vector<int64_t> input_shape;
    public:
    onnx_session(std::string mp, std::vector<int64_t> input_shape)
    
    {
        this->model_path= mp;
        this->input_shape=input_shape;
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, this->model_path.c_str());
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        // 1. Initialize ONNX Session
        session = std::make_unique<Ort::Session>(*env.get(), mp.c_str(), session_options);
        // 2. Debug logs for actual names
        Ort::AllocatorWithDefaultOptions allocator;
        in_name  = session->GetInputNameAllocated(0, allocator).get();
        out_name = session->GetOutputNameAllocated(0, allocator).get();
    }
    Ort::Session* get_session()
    {
        return session.get();
    }
    std::vector<int64_t>* get_input_shape()
    {
        return &input_shape;
    }
    std::string* get_model_path()
    {
        return &model_path;
    }
    const char* get_in_name()
    {
        return in_name.data();
    }
    const char* get_out_name()
    {
        return out_name.data();
    }

};


#endif