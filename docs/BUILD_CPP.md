# C++ 依赖与运行（无 Python）

## 已安装到 `~/deps`（用户目录，无需 sudo）

| 组件 | 来源 |
|------|------|
| hiredis | github.com/redis/hiredis → `~/deps` |
| muduo | github.com/chenshuo/muduo → `~/deps` |
| MySQL / protobuf / boost 头文件 | apt `.deb` 解压到 `~/deps` |

源码缓存在 `thirdparty/hiredis`、`thirdparty/muduo`。

## 运行时配套

```bash
# MySQL（文档表结构）
docker start novachat-mysql   # 127.0.0.1:3307  root/100103  db=chat
docker exec -i novachat-mysql mysql -uroot -p100103 < sql/init.sql

# Redis（原版代码写死 6379）
docker start novachat-redis6379
```

`db.hpp` 中端口为 **3307**（对接上述容器）；密码仍为原版 `100103`。

## 编译 / 启动

```bash
bash scripts/build.sh
bash scripts/run_gateway.sh
# 另开终端（同样需要 LD_LIBRARY_PATH）
export LD_LIBRARY_PATH=$HOME/deps/lib:$HOME/deps/lib/x86_64-linux-gnu
./bin/NovaClient 127.0.0.1 6666
```

种子用户：`1 alice` / `2 bob` / `3 carol`，密码 `123456`。

冒烟：`./bin/smoke_login`（登录 alice）。

## 底色说明

Gateway 仍是原版路径：**muduo 长连接 + 本地 MySQL Model + Redis Pub/Sub 跨机**，未改成 Python，也未把聊天 CRUD 全拆 RPC。
