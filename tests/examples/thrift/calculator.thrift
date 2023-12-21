include "example.thrift"

namespace cpp lts.tutorial

service calculator extends example.example {
    i32 add(1:i32 arg1, 2:i32 arg2),
    i32 subtract(1:i32 arg1, 2:i32 arg2),
    i32 multiply(1:i32 arg1, 2:i32 arg2),
    i32 divide(1:i32 arg1, 2:i32 arg2)
}
