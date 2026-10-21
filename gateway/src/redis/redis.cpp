#include "redis.hpp"
#include <iostream>
using namespace std;

Redis::Redis() : _publish_context(nullptr), _subcribe_context(nullptr) {}
Redis::~Redis()
{
    if (_publish_context) redisFree(_publish_context);
    if (_subcribe_context) redisFree(_subcribe_context);
}

bool Redis::connect()
{
    _publish_context = redisConnect("127.0.0.1", 6379);
    _subcribe_context = redisConnect("127.0.0.1", 6379);
    if (!_publish_context || !_subcribe_context) {
        cerr << "connect redis failed!" << endl;
        return false;
    }
    thread t([&]() { observer_channel_message(); });
    t.detach();
    cout << "connect redis-server success!" << endl;
    return true;
}

bool Redis::publish(int channel, string message)
{
    lock_guard<mutex> lock(_cmd_mu);
    redisReply* reply =
        (redisReply*)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (!reply) return false;
    freeReplyObject(reply);
    return true;
}

bool Redis::subscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(_subcribe_context, "SUBSCRIBE %d", channel)) return false;
    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(_subcribe_context, &done)) return false;
    }
    return true;
}

bool Redis::unsubscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(_subcribe_context, "UNSUBSCRIBE %d", channel)) return false;
    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(_subcribe_context, &done)) return false;
    }
    return true;
}

void Redis::observer_channel_message()
{
    redisReply* reply = nullptr;
    while (REDIS_OK == redisGetReply(_subcribe_context, (void**)&reply)) {
        if (reply && reply->element[2] && reply->element[2]->str) {
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
}

void Redis::init_notify_handler(function<void(int, string)> fn) { _notify_message_handler = fn; }
