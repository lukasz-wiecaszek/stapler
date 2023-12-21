include "example.thrift"

namespace cpp lts.tutorial

/* Operation code
(PING from client -> server, PONG from server -> client) */
enum operation {
    PING,
    PONG
}

/* Basic Data Types */
struct bdt_struct {
    1: bool    v1,
    2: i8      v2,
    3: i16     v3,
    4: i32     v4,
    5: i64     v5,
    6: double  v6,
    7: string  v7,
    8: binary  v8,
}

/* Compound Data Types */
struct cdt_struct {
    1: map<i8, string>  v1,
    2: list<i16>        v2,
    3: set<i32>         v3,
}

/* Main structure encapsulating different data types */
struct test_struct {
    1: operation op,
    2: bdt_struct bdt,
    3: cdt_struct cdt,
}

service ping extends example.example {
    test_struct ping(1: test_struct arg),
    oneway void hello(1: string text)
}
