#include <type_traits>
#include <utility>
#include <tuple>
#include <vector>

#include <set>
#include <iostream>

template<typename T>
struct RuntimeW {
    T* ptr;
};

struct RefHolder;

struct RefBase {
    RefHolder* holder = nullptr;
    std::function<RefHolder*()> connection;
};

// base for structs that contains references
// Self referential, so they can't be copied
struct RefHolder {
    std::vector<RefBase*> ref_ptrs;
    RefHolder() = default;
    RefHolder(const RefHolder& o) = delete;
    RefHolder(RefHolder&& o) = delete;
};


struct Runtime {
    // allocate object
    template<typename T, typename... Args>
    RuntimeW<T> allocate(Args... args) {
        T* ptr = new T(std::forward<T>(args)...);
        // handle ref holders
        if constexpr(std::is_base_of_v<RefHolder, T>) {
            auto set_holder_ptr = [ptr](auto& r) {
                r.holder = ptr;
                ptr->ref_ptrs.push_back((RefBase*) &r);
                return 0;
            };
            std::apply([&set_holder_ptr](auto&... vs) { 
                ((set_holder_ptr(vs)), ...);
            }, ptr->refs());
        }
        return RuntimeW<T>{ptr};
    }
    void addRoot(RefBase* r) {
        roots.insert(r);
    }
    void removeRoot(RefBase* r) {
        roots.erase(r);
    }
    void spread(RefBase* base) {
        std::function<void(RefBase*)> f = [&f](const RefBase* r) {
            if (RefHolder* h = r->connection(); h != nullptr) {
                auto fields = h->ref_ptrs;
                for (auto fd: fields) {
                    f(fd);
                }
            }
        };
        f(base);
    }

private:
    std::set<RefBase*> roots;
} RT;

// references
template<typename T>
struct Ref : RefBase {
    T* value;
    Ref() : RefBase() { connection = [](){return nullptr;}; }
    Ref(const Ref<T>& o) {};
    Ref(const Ref<T>&& o) = delete;
    Ref(RuntimeW<T> w) { *this = w; }
    ~Ref() {}
    void operator=(Ref<T>&& o) { value = o.value; o.value = nullptr; }
    void operator=(RuntimeW<T> w) {
        value = w.ptr;
        if (this->holder == nullptr) { // check if this is a top level reference
            RT.addRoot(this);
        }
        if constexpr(std::is_base_of_v<RefHolder, T>) {
            connection = [this](){return (RefHolder*) value;};
        } else {
            connection = [this](){return nullptr; };
        }
    }
    T* operator->() { return value; }
};

struct Wheel {};
struct Car : RefHolder {
    Car(): RefHolder() {}
    Ref<Wheel> w1, w2;
    Ref<int> year;
    auto refs() { return std::tie(w1, w2, year); }
};

int main() {
    Ref<Car> r1 = RT.allocate<Car>();
    Ref<int> r2 = RT.allocate<int>();

    RT.spread(&r1);
    RT.spread(&r2);
    // RefHolder [refs] -> Ref -> connection?... -> RefHolder [refs]
}