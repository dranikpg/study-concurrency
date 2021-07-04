use std::sync::*;
use std::sync::atomic::*;
use std::thread::spawn;

#[test]
fn atomic() {
    static COUNTER: AtomicI64 = AtomicI64::new(0);
    let mut handles = vec![];
    for _ in 0..5 {
        handles.push(spawn(|| {
            for _ in 0..10 {
                COUNTER.fetch_add(1, Ordering::Relaxed);
            }
        }));   
    }
    for h in handles { h.join().unwrap(); }
    assert_eq!(COUNTER.load(Ordering::Relaxed), 50);
}

#[test]
fn once() {
    static VALUE: AtomicI64 = AtomicI64::new(0);
    static ONCE: Once = Once::new();
    let mut handles = vec![];
    for _ in 0..5 {
        handles.push(spawn(||{
            ONCE.call_once(||{
                VALUE.fetch_add(5, Ordering::SeqCst);
            })
        }));
    }
    for h in handles { h.join().unwrap(); }
    assert_eq!(VALUE.load(Ordering::SeqCst), 5);
}

#[test]
fn mutex() {
    let mutex = Arc::new(Mutex::new(0i32));
    let mut handles = vec![];
    for _ in 0..5 {
        let mcopy = mutex.clone();
        handles.push(spawn(move ||{
            for _ in 0..10 {
                let mut lock = mcopy.lock().unwrap();
                *lock += 1;
            }
        }));
    }
    for h in handles { h.join().unwrap(); }
    assert_eq!(*mutex.lock().unwrap(), 50);
}

#[test]
fn barrier() {
    static VALUE: AtomicI64 = AtomicI64::new(0);
    let barrier = Arc::new(Barrier::new(5));
    let mut handles = vec![];
    for _ in 0..5 {
        let barrier = barrier.clone();
        handles.push(spawn(move ||{
            for _ in 0..5 {
                VALUE.fetch_add(1, Ordering::SeqCst);
                barrier.wait();
                assert_eq!(VALUE.load(Ordering::Relaxed), 5);
                barrier.wait();
                VALUE.fetch_add(-1, Ordering::Relaxed);
            }
        }));
    }
    for h in handles { h.join().unwrap(); }
}

#[test]
fn condvar() {
    let pair = Arc::new((Mutex::new(false), Condvar::new()));
    let pair2 = Arc::clone(&pair);
    spawn(move|| {
        let (lock, cvar) = &*pair2;
        let mut started = lock.lock().unwrap();
        *started = true;
        // We notify the condvar that the value has changed.
        cvar.notify_one();
    });
    let (lock, cvar) = &*pair;
    let mut started = lock.lock().unwrap();
    while !*started {
        started = cvar.wait(started).unwrap();
    }
    assert_eq!(*started, true);
}

#[test]
fn channel() {
    let (sx, rx) = mpsc::sync_channel(0);
    spawn(move ||{
        sx.send(11).unwrap();
    });
    assert_eq!(rx.recv().unwrap(), 11);
}
