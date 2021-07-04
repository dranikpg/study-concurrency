const DEFAULT_STACK_SIZE: usize = 2 * 1000; // two kilobytes

type FuncType = fn(usize, usize);

#[link(name = "nasm")]
extern "C" {
    fn nasm_init(stack_base: usize, func: FuncType) -> usize;
    fn nasm_switch(old_rsp_w: *mut usize, new_rsp_r: usize, rdi: usize, rsi: usize);
}

pub struct Stack {
    pub bottom: usize,
    pub top: usize,
    align: u8,
    allocated: bool,
}

static mut GLOBAL_STACK: Stack = Stack {bottom: 0, top: 0, align: 0, allocated: false};

impl Stack {
    pub fn init(f: FuncType) -> Self {
        let mut bottom = Box::into_raw(Box::new([0u8; DEFAULT_STACK_SIZE])) as usize;
        bottom += DEFAULT_STACK_SIZE;
        let align = (bottom % 16) as u8;
        bottom -= align as usize;
        let top = Self::init_stack(bottom, f);
        Stack { bottom, top, align, allocated: true }
    }

    pub fn switch<T>(from: &mut Stack, to: &Stack, arg1: T, arg2: T)
        where T: Into<Option<usize>> {
        unsafe {
            nasm_switch(&mut from.top as *mut usize, to.top,
                        arg1.into().unwrap_or(0),
                        arg2.into().unwrap_or(0));
        }
    }

    pub fn switch_to<T>(to: &Stack, arg1: T, arg2: T)
    where T: Into<Option<usize>> {
        unsafe { Self::switch(&mut GLOBAL_STACK, to, arg1, arg2); }
    }

    pub fn terminate() {
        let mut empty =  Stack {bottom: 0, top: 0, align: 0, allocated: false};
        unsafe { Self::switch(&mut empty, &GLOBAL_STACK, None, None) };
    }

    fn init_stack(bottom: usize, f: FuncType) -> usize {
        unsafe {
            nasm_init(bottom, f)
        }
    }
}

impl Drop for Stack {
    fn drop(&mut self) {
        if self.allocated {
            let ptr = self.bottom + self.align as usize - DEFAULT_STACK_SIZE;
            unsafe { Box::from_raw(ptr as *mut [u8; DEFAULT_STACK_SIZE]) };
        }
    }
}
