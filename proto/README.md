# 业务 IDL

```bash
# 在 NovaChat 根目录
protoc -I proto --cpp_out=proto \
  proto/common.proto proto/user.proto proto/friend.proto proto/message.proto
```

生成的 `*.pb.cc / *.pb.h` 建议继续放在 `proto/`，由 services 与 gateway 共同链接。

当前仓库只提交 `.proto` 草图，避免强依赖本机 protoc 版本；接入编译时再生成。
