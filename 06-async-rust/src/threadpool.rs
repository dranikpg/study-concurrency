pub trait Func = Fn() + Send + 'static;

enum Task {
    Task(Box<dyn Fn() + Send>),
    Terminate
}

pub struct Pool {
    threads: Vec<std::thread::JoinHandle<()>>,
    sender: crossbeam_channel::Sender<Task>
}

pub struct PoolHandle<'a> (&'a crossbeam_channel::Sender<Task>);

impl<'a> Pool {
    pub fn new(num_threads: usize) -> Self {
        let (sx, rx) = crossbeam_channel::unbounded();
        let threads = (0..num_threads)
            .map(|_| rx.clone())
            .map(|rx| std::thread::spawn(move || Self::worker(rx)))
            .collect();
        Pool {
            sender: sx,
            threads
        }
    }
    fn worker(rx: crossbeam_channel::Receiver<Task>) {
        while let Ok(Task::Task(func)) = rx.recv() {
            func();
        }
    }
    pub fn handle(&'a self) -> PoolHandle<'a> {
        PoolHandle (&self.sender)
    }
}

impl Drop for Pool {
    fn drop(&mut self) {
        for _ in &self.threads { self.sender.send(Task::Terminate).ok(); }
        std::mem::drop(&mut self.sender);
        for t in self.threads.drain(..) { t.join().ok(); }
    }
}

impl<'a> PoolHandle<'a> {
    pub fn send(&self, func: impl Func) {
        self.send_boxed(Box::new(func));
    }
    pub fn send_boxed(&self, func: Box<dyn Func>) {
        self.0.send(Task::Task(func)).ok();    
    }
}