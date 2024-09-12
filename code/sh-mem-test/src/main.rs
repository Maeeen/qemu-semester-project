use shared_memory::{ Shmem, ShmemConf };
use subprocess::Exec;
use std::mem::ManuallyDrop;

pub fn main() {
    const BITMAP_SIZE: u64 = 65536;
    const SHMEM_SIZE: u64 = BITMAP_SIZE;

    println!("Creating shared memory of size: {}", SHMEM_SIZE);
    // Setup shared memory
    let shmem = ShmemConf::new().size(SHMEM_SIZE as usize)
        .os_id("fuzz-qemu-plugin")
        .create()
        .expect("Failed to create shared memory");

    let signals: *mut u8 = shmem.as_ptr();
    unsafe { 
        *signals = 15;
    }

    println!("Running the plugin");

    // Setup the subprocess
    let s = Exec::shell("make -C /work/my-plugin TARGET_BIN=hello run")
        .stdout(subprocess::Redirection::Pipe)
        .stderr(subprocess::Redirection::Pipe)
        .stdin("Marwan")
        // .stdin(subprocess::Redirection::Pipe)
        .capture().expect("Failed to execute command");
    println!("stdout: {}", s.stdout_str());


}