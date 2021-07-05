use std::rc::Rc;
use std::cell::Cell;
use std::collections::VecDeque;
mod stack;

#[derive(Copy, Clone)]
enum FiberState {
    Running,
    Suspended,
    Finished
}

struct Fiber {
    stack: stack::Stack,
    state: Cell<FiberState>,
    args: (usize, usize)
}

#[derive(Default)]
struct Executor {
    queue: VecDeque<Rc<Fiber>>,
    stack: stack::Stack,
    current: Option<Rc<Fiber>>
}

impl Executor {
    pub fn enqueue<T>(&mut self, f: T) 
    where T: Fn() + 'static {
        self.queue.push_back(Rc::new(Fiber {
            stack: stack::Stack::init(Self::trampoline),
            state: Cell::new(FiberState::Suspended),
            args: unsafe {std::mem::transmute(&f as &dyn Fn())}
        }));
    }

    fn run(&mut self) {
        while let Some(fiber) = self.queue.pop_front() {
            self.current = Some(fiber.clone());
            match self.switch_to(&fiber) {
                FiberState::Finished => continue,
                FiberState::Suspended => self.queue.push_back(fiber),
                _ => unreachable!()
            };
        }
    }

    fn trampoline(args: (usize, usize)) -> ! {
        let func_ptr: &dyn Fn() = unsafe {std::mem::transmute(args)};
        func_ptr();
        unsafe {EXECUTOR.as_mut().unwrap().suspend(FiberState::Finished);}
        unreachable!();
    }

    fn suspend(&mut self, s: FiberState) {
        if let Some(ref fiber) = self.current {
            fiber.state.set(s);
            stack::Stack::switch(&fiber.stack, &self.stack, (0,0));
        }
    }

    fn switch_to(&self, fiber: &Fiber) -> FiberState {
        fiber.state.set(FiberState::Running);
        stack::Stack::switch(&self.stack, &fiber.stack, fiber.args);
        fiber.state.get()
    }
}

static mut EXECUTOR: Option<Executor> = None;

fn suspend() {
    unsafe {EXECUTOR.as_mut().unwrap().suspend(FiberState::Suspended); }
}

fn enqueue(f: impl Fn() + 'static) {
    unsafe { EXECUTOR.as_mut().unwrap().enqueue(f); }
}

fn f2() {
    println!(" 2 - 1");
    suspend();
    println!(" 2 -   2");
    suspend();
    println!(" 2 -     3");
}

fn f1(){
    println!("1  - 1");
    enqueue(f2); 
    suspend();    
    println!("1  -   2");
    suspend();
    println!("1  -     3");
}

/* The example prints:
1  - 1
 2 - 1
1  -   2
 2 -   2
1  -     3
 2 -     3
*/

fn main() { 
    unsafe {
        EXECUTOR = Some(Executor::default());
        enqueue(f1);
        EXECUTOR.as_mut().unwrap().run();
        EXECUTOR = None;
    }
}
