mod stack;
use stack::Stack;
fn f1(p1: usize, p2: usize) {
    let ptr: &dyn Fn() = unsafe {std::mem::transmute((p1, p2))};
    ptr();
    Stack::terminate();
}

fn main() {
    let i = 11;
    let mut lambda = || {
        println!("Hello from lambda {}", i);
    };
    let (p1, p2): (usize, usize) = {
        let dynptr: &dyn Fn() = &lambda;
        unsafe {std::mem::transmute(dynptr)}
    };
    let stack = stack::Stack::init(f1);
    Stack::switch_to(&stack,
                     p1,
                     p2);
    println!("Works!");
}
