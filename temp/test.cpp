#include <iostream>
using namespace std;
class MyClass {
public:
    int value;

    explicit MyClass(int val) : value(val) {}

    // 常量成员函数
    int getValue() const {
        // this 指针在这里是 const 类型的，因为它不能调用任何非常量成员函数
        return value; // 正确：getValue 是 const 成员函数
    }

    // 非常量成员函数
    void setValue(int val) {
        // this 指针在这里不是 const 类型的，因为它可以调用任何成员函数
        value = val; // 正确：setValue 不是 const 成员函数
    }
};

int main() {
    MyClass obj(10); // obj 是非 const 对象
    int val = obj.getValue(); // 正确：getValue 是 const 成员函数，可以在非 const 对象上调用
    obj.setValue(20); // 错误：setValue 不是 const 成员函数，不能在 const 对象上调用
    // 但是可以在非 const 对象上调用，所以这里的调用是正确的
    obj.setValue(20); 

    const MyClass constObj(30); // constObj 被声明为 const 对象
    int val2 = constObj.getValue(); // 正确：getValue 可以在 const 对象上调用
    // constObj.setValue(40); // 错误：setValue 不能在 const 对象上调用

    return 0;
}