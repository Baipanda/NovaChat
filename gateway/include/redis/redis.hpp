#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
using namespace std;

class Redis
{
public:
    Redis();
    ~Redis();
    bool connect();
    bool publish(int channel, string message);
    bool subscribe(int channel);
    bool unsubscribe(int channel);
    void observer_channel_message();
    void init_notify_handler(function<void(int, string)> fn);

private:
    redisContext* _publish_context;
    redisContext* _subcribe_context;
    mutex _cmd_mu;
    function<void(int, string)> _notify_message_handler;
};
#endif
