#include <iostream>
#include <vector>
#include <thread>
#include <functional>
#include <chrono>
#include <memory>
#include <future>
using namespace std;
// class A : public enable_shared_from_this<A>
// {
// public:
//     int a_i = 4;
//     function<void(shared_ptr<A>)> cb_;
//     A(function<void(shared_ptr<A>)> cb) { 
//         cout << "A构造" << endl; 
//         cb_ = std::move(cb);
//     }
//     A(const A &a) {
//         cout << "A拷贝" << endl;
//     }
//     ~A() {
//         cout << "A析构" << endl;
//     }
//     void testfunc() {
//         cb_(shared_from_this());
//     }
// };

// class B 
// {
// public:
//     int b_i = 8;
//     vector<shared_ptr<A>> vec_;
//     B() { 
//         cout << "B构造" << endl; 
//         shared_ptr<A> a_ptr(new A(bind(&B::callback, this, placeholders::_1)));
//         vec_.emplace_back(a_ptr);
        
//     }
//     B(const B &a) {
//         cout << "B拷贝" << endl;
//     }
//     ~B() {
//         cout << "B析构" << endl;
//     }

//     void testBfunc() {
//         vec_[0]->testfunc();
//     }

//     void callback(shared_ptr<A> aptr)
//     {
//         cout << "aptr use_count: " << aptr.use_count() << endl;
//         // cout << "aptr->a_q = " << aptr->a_i << endl;
//         auto iter = vec_.begin();
//         vec_.erase(iter);
//         cout << "aptr use_count: " << aptr.use_count() << endl;
//     }
// };

int func(int a, int b)
{
    cout << a << " " << b << endl;
    return 3;
}

// 创建的临时对象通过make_shared变成智能指针，会调用拷贝构造函数吗
// 多个智能指针之间赋值会调用拷贝构造函数吗
// 类中有智能指针，用智能指针再包裹类会调用拷贝构造函数吗
// 子线程执行完了才join会出错吗？不会
// 

int main()
{
    
    std::packaged_task<int()> pt1(std::bind(func, 1, 2));
    function<void()> f(std::move(pt1));
	//声明一个std::future对象fu1，包装Test_Fun的返回结果类型，并与pt1关联
	f();
 
 
	return 1;
}