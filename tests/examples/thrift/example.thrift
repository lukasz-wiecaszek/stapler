
namespace cpp lts.tutorial

struct version_struct {
    1: i32 major,
    2: i32 minor,
    3: i32 micro
}

service example {
    string name(),
    version_struct version(),
}
