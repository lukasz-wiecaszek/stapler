include "example.thrift"

namespace cpp lts.tutorial

typedef i64 timer_id

const i64 INVALID_TIMER_ID = -1

struct timestamp {
    1: i64 counter
    2: i64 abstime
}

service timer extends example.example {
    timer_id start(1: i64 interval_us),
    void stop(1: timer_id id),

    /**
     * Because Apache Thrift doesn't support arbitrary message
     * broadcasting/signaling known from other rpc frameworks
     * (DBbus signals, Fushia events, Franca broadcasts),
     * we will apply polling here.
     * Once timer is started, tick() method will block and return only
     * when the specified interval elapeses.
     * Let's assume that interval is set to 100 ms.
     * If the first call to tick() is done 320 ms after timer is started,
     * the tick() method will return 3 times immediately,
     * and fourth time it will block for 80 ms and then return.
     */
    timestamp tick(1: timer_id id)
}
