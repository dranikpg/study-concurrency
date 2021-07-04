use std::rc::Rc;
use std::cell::{Cell, RefCell, Ref};
use std::collections::VecDeque;
use crate::FiberState::Suspended;

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

fn trampoline(args: (usize, usize)) -> ! {
    let func_ptr: &dyn Fn() = unsafe {std::mem::transmute(args)};
    func_ptr();
    suspend(FiberState::Finished);
    unreachable!();
}

impl Executor {
    fn start(&mut self) {
        self.run();
    }

    pub fn launch<T>(&mut self, f: T)
        where T: Fn() + 'static {
        let args = unsafe {
            let ptr: &dyn Fn() = &f;
            std::mem::transmute(ptr)
        };
        let fiber = Rc::new(Fiber {
            stack: stack::Stack::init(trampoline),
            state: Cell::new(FiberState::Suspended),
            args
        });
        self.queue.push_back(fiber.clone());
    }

    fn suspend(&mut self, s: FiberState) {
        match self.current {
            Some(ref fiber) => {
                fiber.state.set(s);
                stack::Stack::switch(&fiber.stack, &self.stack, (0,0));
            },
            _ => unreachable!()
        }
    }

    fn run(&mut self) {
        loop {
            let fiber: Option<Rc<Fiber>> = self.queue.pop_front();
            match fiber {
                Some(fiber) => {
                    fiber.state.set(FiberState::Running);
                    self.current = Some(fiber.clone());
                    self.switch_to(&fiber);
                    match fiber.state.get() {
                        FiberState::Finished => continue,
                        FiberState::Suspended => self.queue.push_back(fiber),
                        _ => unreachable!()
                    };
                },
                _ => break
            }
        }
    }

    fn switch_to(&self, fiber: &Fiber) {
        stack::Stack::switch(
            &self.stack,
            &fiber.stack,
            fiber.args
        );
    }
}

static mut GE: Option<Executor> = None;

fn suspend(s: FiberState) {
    unsafe {
        match GE {
            Some(ref mut g) => {
                g.suspend(s)
            },
            _ => unreachable!()
        }
    }
}

fn launch<T>(f: T)
    where T: Fn() + 'static {
    unsafe {
        match &mut GE {
            Some(ref mut g) => {
                g.launch(f)
            },
            _ => unreachable!()
        }
    };
}

fn f2() {
    println!("F2 part 1");
    suspend(Suspended);
    println!("F2 part 2");
}
fn f1(){
    launch(f2);
    println!("F1 - part 1");
    suspend(Suspended);
    println!("F1 - part 2");
    suspend(Suspended);
    println!("F1 - part 3");
}

fn main() {
    unsafe {
        GE = Some(Executor::default());
        let mut g = GE.as_mut().unwrap();
        g.launch(f1);
        g.start();
        GE = None;
    }
}
