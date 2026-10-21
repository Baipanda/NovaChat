#pragma once

#include "rpcerror.h"

#include <google/protobuf/service.h>
#include <string>

class MprpcController : public google::protobuf::RpcController
{
public:
    void Reset() override
    {
        failed_ = false;
        errcode_ = RpcErr::kOk;
        error_text_.clear();
    }

    bool Failed() const override { return failed_; }
    std::string ErrorText() const override { return error_text_; }

    void StartCancel() override {}
    bool IsCanceled() const override { return false; }
    void NotifyOnCancel(google::protobuf::Closure*) override {}

    void SetFailed(const std::string& reason) override
    {
        failed_ = true;
        error_text_ = reason;
        if (errcode_ == RpcErr::kOk) errcode_ = RpcErr::kParseFail;
    }

    void SetError(int code, const std::string& text = "")
    {
        failed_ = (code != RpcErr::kOk);
        errcode_ = code;
        error_text_ = text.empty() ? RpcErr::ToString(code) : text;
    }

    int ErrorCode() const { return errcode_; }

private:
    bool failed_ = false;
    int errcode_ = RpcErr::kOk;
    std::string error_text_;
};
