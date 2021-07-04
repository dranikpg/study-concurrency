use std::sync::atomic::AtomicI64;
use std::sync::atomic::Ordering;
use crossbeam::sync::{Parker};

#[test]
fn scope () {
    let counter = AtomicI64::new(0);
    crossbeam::scope(|s| {
        for _ in 0..5 {
            s.spawn(|_|{
                counter.fetch_add(1, Ordering::SeqCst);
            });
        }
    }).unwrap();
    assert_eq!(counter.load(Ordering::SeqCst), 5);
}

#[test]
fn parker() {
    static counter: AtomicI64 = AtomicI64::new(0);
    let p = Parker::new();
    let u = p.unparker().clone();
    let h = std::thread::spawn(move ||{
        for _ in 0..10 {
            counter.fetch_add(1, Ordering::Relaxed);
            std::thread::sleep(std::time::Duration::from_millis(10));
        }
        u.unpark();
    });
    p.park(); // awaits unparking
    assert_eq!(counter.load(Ordering::Relaxed), 10);
    h.join();
}
