use std::cell::Cell;

const DEFAULT_STACK_SIZE: usize = 10 * 1000; // ten kilobytes

#[link(name = "nasm")]
extern "C" {
    fn nasm_init(stack_base: usize, func: FuncType) -> usize;
    fn nasm_switch(old_rsp_w: *mut usize, new_rsp_r: usize, rdi: usize, rsi: usize);
}

type FuncType = fn((usize, usize)) -> !;

#[derive(Default)]
pub struct Stack {
    mem: Option<Vec<u8>>,
    top: Cell<usize>
}

impl Stack {
    pub fn init(f: FuncType) -> Self {
        let mem = vec![0u8; DEFAULT_STACK_SIZE];
        let bottom = {
            let ptr = (&mem[0] as *const u8) as usize + DEFAULT_STACK_SIZE;
            ptr - (ptr % 16)
        };
        let top = Cell::new(Self::init_stack(bottom, f));
        Stack { mem: Some(mem), top }
    }

    pub fn switch(from: &Stack, to: &Stack, (arg1, arg2): (usize, usize)) {
        unsafe {
            nasm_switch(from.top.as_ptr(), to.top.get(), arg1, arg2);
        }
    }

    fn init_stack(bottom: usize, f: FuncType) -> usize {
        unsafe {
            nasm_init(bottom, f)
        }
    }
}
