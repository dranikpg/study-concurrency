#![feature(trait_alias)]
#![feature(map_first_last)]

use std::{pin::Pin, sync::atomic::{AtomicBool, Ordering::SeqCst}, task::{Poll, RawWaker, RawWakerVTable, Waker}, time::Duration};
use futures::Future;

mod threadpool;
mod sleeper;

// Schedule flag

static sched: AtomicBool = AtomicBool::new(false);

// Waker VTable

fn empty(_: *const ()){}

fn wake(_: *const ()) {
    sched.store(true, SeqCst);
}

fn build_raw_waker(_: *const ()) -> RawWaker {
    static VTABLE: RawWakerVTable = RawWakerVTable::new(
        build_raw_waker,
        wake,
        wake,
        empty
    );
    RawWaker::new(std::ptr::null(), &VTABLE)
}

// Test

struct TimerFuture<'a> {
    sleeper: &'a sleeper::Sleeper<'a>,
    slept: bool,
    duration: Duration
}

impl<'a> TimerFuture<'a> {
    pub fn new(duration: Duration, s: &'a sleeper::Sleeper<'a>) -> Self {
        TimerFuture {
            duration, 
            slept: false,
            sleeper: s
        }
    }
}

impl<'a> Future for TimerFuture<'a> {
    type Output = ();
    fn poll(self: Pin<&mut Self>, cx: &mut std::task::Context<'_>) -> Poll<Self::Output> {
        match self.slept {
            true => Poll::Ready(()),
            false => {
                let waker_c = cx.waker().clone();
                self.sleeper.run(self.duration, move || {
                    waker_c.wake_by_ref();
                });
                unsafe {self.get_unchecked_mut().slept = true};
                Poll::Pending
            }
        }
    }
}

async fn sleep_test<'a>(s: &'a sleeper::Sleeper<'a>) {
    println!("sleeping...");
    TimerFuture::new(Duration::from_secs(1), s).await;
    println!("one sec");
    TimerFuture::new(Duration::from_secs(1), s).await;
    println!("two sec");
}

fn main() {
    let t = threadpool::Pool::new(2);
    let s = sleeper::Sleeper::new(t.handle());

    let mut fut = sleep_test(&s);
    
    let waker = unsafe {Waker::from_raw(build_raw_waker(std::ptr::null()))};
    let mut context = std::task::Context::from_waker(&waker);

    sched.store(true, SeqCst);

    loop {
        if sched.load(SeqCst) {
            sched.store(false, SeqCst);
            if let Poll::Ready(()) = unsafe {Pin::new_unchecked(&mut fut)}.poll(&mut context) {
                break
            }
        }
        std::hint::spin_loop();
    }

}