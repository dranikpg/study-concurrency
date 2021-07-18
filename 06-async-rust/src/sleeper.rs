use crate::threadpool::PoolHandle;
use std::collections::BTreeSet;
use std::marker::PhantomData;
use std::sync::Arc;
use std::sync::atomic::Ordering::SeqCst;
use std::sync::{Mutex, atomic::AtomicBool};
use std::time::{Instant, Duration};
use std::cmp::{Eq, Ord, Ordering, PartialEq, PartialOrd};

pub trait Func = Fn() + Send + 'static;

struct Task {
    func: Box<dyn Func>,
    instant: Instant 
}

impl PartialEq for Task {
    fn eq(&self, other: &Self) -> bool {
        &*self.func as *const dyn Func == &*other.func as *const dyn Func
    }
}

impl Eq for Task {}

impl PartialOrd for Task {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.instant.cmp(&other.instant))
    }
}

impl Ord for Task {
    fn cmp(&self, other: &Self) -> Ordering {
        self.instant.cmp(&other.instant)
    }
}

struct Shared {
    queue: Mutex<BTreeSet<Task>>,
    terminated: AtomicBool,
    pool: PoolHandle<'static>
}

pub struct Sleeper<'a> {
    shared: Arc<Shared>,
    thread: Option<std::thread::JoinHandle<()>>,
    _lifetime: std::marker::PhantomData<&'a ()>
}

impl<'a> Sleeper<'a> {
    pub fn new(handle: PoolHandle<'a>) -> Self {
        let shared = Arc::new(Shared{
            queue: Mutex::new(BTreeSet::new()),
            terminated: AtomicBool::default(),
            pool: unsafe {std::mem::transmute::<PoolHandle<'a>, PoolHandle<'static>> (handle) }
        });
        let shared_c = shared.clone();
        Sleeper {
            shared,
            thread: Some(std::thread::spawn(move || Self::worker(shared_c))),
            _lifetime: PhantomData::default()
        }
    }
    fn worker(shared: Arc<Shared>) {
        loop {
            let mut lock = shared.queue.lock().unwrap();
            if shared.terminated.load(SeqCst) && lock.is_empty() { return; }
            let now = Instant::now();
            while let Some(task) = lock.first() {
                let task = match task.instant.cmp(&now) {
                    Ordering::Greater => break,
                    _ => lock.pop_first()
                };
                if let Some(task) = task {
                    shared.pool.send_boxed(task.func);
                }
            }
            drop(lock);
            std::thread::sleep(Duration::from_millis(5));
            std::hint::spin_loop();
        }
    }
    pub fn run(&self, delay: Duration, func: impl Func) {
        let mut g = self.shared.queue.lock().unwrap();
        g.insert(Task {
            func: Box::new(func),
            instant: Instant::now() + delay
        });
    }
}

impl<'a> Drop for Sleeper<'a> {
    fn drop(&mut self) {
        self.shared.terminated.store(true, SeqCst);
        self.thread.take().unwrap().join().ok();
    }
}

