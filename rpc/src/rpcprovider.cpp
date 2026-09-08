#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"

/*
service* 记录服务对象
    pserviceDesc=service->GetDescriptor()  pserivceDesc->service_name是service描述
        pmethodeDesc=pserviceDesc->method(i)是method方法  pmethodDesc->name()是method描述
        
*/

void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;
    
    // 获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor* pserviceDesc = service->GetDescriptor();
    // 获取服务的名字
    std::string service_name = pserviceDesc->name();
    // 获取服务对象service的方法的数量
    int methodCnt = pserviceDesc->method_count();

    std::cout << "service_name: " << service_name << std::endl;

    for(int i=0; i<methodCnt; ++i)
    {
        // 获取了服务对象指定下标的服务方法的描述（抽象描述）
        const google::protobuf::MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

// 处理新的socket连接
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if(!conn->connected())
    {
        // 连接断开
        conn->shutdown();
    }
}

/*
    在框架内部 RpcProvider 和 RpcConsumer 协商好之间通信使用的protobuf数据类型:
        service_name
        method_name
        args
    定义proto message类型，进行数据的序列化和反序列化
*/

// 已建立连接用户的读写事件回调，如果有一个rpc服务的调用请求，那么OnMessage方法就会相应
// 网络中消息来了之后，从buffer中读上来
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn, 
                            muduo::net::Buffer* buffer, 
                            muduo::Timestamp)
{
    std::string recv_buf = buffer->retrieveAllAsString();
    uint32_t header_size = 0;
    // 拷贝四个字节的内容
    recv_buf.copy((char*)&header_size, 4, 0);
    // 根据header_size读取数据头的原始字符流，反序列化数据得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    
    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        // 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.methode_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        // 数据库反序列化失败
        std::cout << "rpc_header_str: " << rpc_header_str <<"parse error!" << std::endl;
    }
    
    std::string args_str = recv_buf.substr(4 + header_size, args_size);
    // 打印调试信息
    std::cout << "==================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;    
    std::cout << "args_str: " << args_str << std::endl;        
    std::cout << "==================================" << std::endl;

    // 获取service对象和method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end())
    {
        std::cout << service_name << "does not exist!" << std::endl;
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end())
    {
        std::cout << service_name << ": " << method_name << "does not exist!" << std::endl;
        return;
    }
    
    google::protobuf::Service* service = it->second.m_service;          // 获取service对象
    const google::protobuf::MethodDescriptor* method = mit->second;     // 获取method对象

    // 生成 rpc 方法调用的请求 request 和 response 参数
    google::protobuf::Message* request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        std::cout << "request parse error, content" << args_str << std::endl;
    }
    google::protobuf::Message* response = service->GetResponsePrototype(method).New();

    // 在框架上根据远端rpc请求调用当前rpc节点上发布的方法
    
    // 绑定一个Closure
    google::protobuf::Closure* done = 
        google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr&, google::protobuf::Message*>
            (this, &RpcProvider::SendRpcResponse, conn, response);

    // UserService对象，在调用Lgion方法
    // new UserService().Login(controller, request, done)
    service->CallMethod(method, nullptr, request, response, done);
}

void RpcProvider::Run()
{
    // 启动TcpServer
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    // 创建 TcpServer 对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");
    
    // 绑定连接回调和消息读写回调方法
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    
    // ========== ZooKeeper 服务注册 ==========
    m_zkCli.Start();
    // /NovaChat-service 根节点
    m_zkCli.Create("/NovaChat-service", nullptr, 0);
    for (auto& sp : m_serviceMap) {
        // /NovaChat-service/PresenceServiceRpc
        std::string service_path = "/NovaChat-service/" + sp.first;
        m_zkCli.Create(service_path.c_str(), nullptr, 0);
        // /NovaChat-service/PresenceServiceRpc/127.0.0.1:8010  临时节点
        std::string host = ip + ":" + std::to_string(port);
        std::string node_path = service_path + "/" + host;
        m_zkCli.Create(node_path.c_str(), host.c_str(), static_cast<int>(host.size()), ZOO_EPHEMERAL);
    }

    std::cout << "RpcProvider start service at ip: " << ip << " port: " << port << std::endl;

    // 设置muduo库的线程数量
    server.setThreadNum(4);

    // 启动网络服务(epoll_wait)
    server.start();
    m_eventLoop.loop();
}

void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response)
{
    std::string response_str;
    if(response->SerializePartialToString(&response_str))  // response进行序列化
    {
        // 序列化成功后，通过网络把rpc方法执行的结果发回给rpc的调用方
        conn->send(response_str);
    }
    else
    {
        std::cout << "serialize response_str error!" << std::endl;
    }
    conn->shutdown();  // 模拟http的短链接服务，由rpcprovider主动断开连接

}