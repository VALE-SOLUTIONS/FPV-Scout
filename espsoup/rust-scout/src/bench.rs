// SPDX-License-Identifier: GPL-3.0-or-later
use crate::composite::{Frame, BOUNCE, NTSC, PAL, PATTERN_COUNT};
use core::ptr;

const OK: i32 = 0;
const DAC_WIDTH: u32 = 2;

extern "C" {
    fn scout_bench_hw_open(capacity: usize, buffer: *mut *mut u8) -> i32;
    fn scout_bench_hw_start(length: usize, blank: u8) -> i32;
    fn scout_bench_hw_pause() -> i32;
    fn scout_bench_hw_close() -> i32;
}

// Matches scout_bench_view_t. The UI task owns the mode and is the only caller.
#[repr(C)]
pub struct View {
    frame: Frame,
    pattern: u32,
    error: i32,
    running: bool,
}

impl View {
    const EMPTY: Self = Self { frame: Frame::EMPTY, pattern: 0, error: OK, running: false };

    unsafe fn close(&mut self) -> bool {
        self.running = false;
        let error = scout_bench_hw_close();
        if error != OK {
            self.error = error;
            return false;
        }
        *self = Self::EMPTY;
        true
    }

    unsafe fn play(&mut self, mode: u32, tick: u32) -> i32 {
        self.frame = Frame::new(self.frame.dma, mode, DAC_WIDTH);
        self.frame.write_sync();
        self.frame.write_pattern(self.pattern, tick);
        let error = scout_bench_hw_start(self.frame.bytes(), self.frame.blank);
        self.running = error == OK;
        error
    }

    unsafe fn redraw(&self) {
        self.frame.write_pattern(self.pattern, COUNTER);
    }

    unsafe fn handle_error(&mut self, error: i32) {
        if error != OK && self.close() {
            self.error = error;
        }
    }
}

#[no_mangle]
pub static mut scout_bench: View = View::EMPTY;
static mut COUNTER: u32 = 0;
static mut NEXT_TICK: i64 = 0;

#[no_mangle]
pub unsafe extern "C" fn scout_bench_open() {
    let view = &mut *ptr::addr_of_mut!(scout_bench);
    if !view.close() {
        return;
    }
    COUNTER = 0;
    NEXT_TICK = 0;
    let capacity = Frame::new(ptr::null_mut(), PAL, DAC_WIDTH).bytes();
    let mut error = scout_bench_hw_open(capacity, &mut view.frame.dma);
    if error == OK {
        error = view.play(NTSC, COUNTER);
    }
    view.handle_error(error);
}

#[no_mangle]
pub unsafe extern "C" fn scout_bench_close() -> bool {
    (*ptr::addr_of_mut!(scout_bench)).close()
}

#[no_mangle]
pub unsafe extern "C" fn scout_bench_select(pattern: u32) {
    let view = &mut *ptr::addr_of_mut!(scout_bench);
    if view.running && pattern < PATTERN_COUNT {
        view.pattern = pattern;
        view.redraw();
    }
}

#[no_mangle]
pub unsafe extern "C" fn scout_bench_next() {
    scout_bench_select((scout_bench.pattern + 1) % PATTERN_COUNT);
}

#[no_mangle]
pub unsafe extern "C" fn scout_bench_format(mode: u32) {
    let view = &mut *ptr::addr_of_mut!(scout_bench);
    if !view.running || mode == view.frame.mode || mode > PAL {
        return;
    }
    view.running = false;
    let mut error = scout_bench_hw_pause();
    if error == OK {
        error = view.play(mode, COUNTER);
    }
    view.handle_error(error);
}

#[no_mangle]
pub unsafe extern "C" fn scout_bench_levels(black: u32, white: u32) -> bool {
    let view = &mut *ptr::addr_of_mut!(scout_bench);
    if !view.running || !view.frame.set_levels(black, white) {
        return false;
    }
    view.redraw();
    true
}

#[no_mangle]
pub unsafe extern "C" fn scout_bench_step(now: i64) {
    let view = &*ptr::addr_of!(scout_bench);
    if view.running && view.pattern == BOUNCE && now >= NEXT_TICK {
        COUNTER = COUNTER.wrapping_add(1);
        view.redraw();
        NEXT_TICK = now.saturating_add(33000);
    }
}
