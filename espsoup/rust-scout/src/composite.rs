// SPDX-License-Identifier: GPL-3.0-only
// Composite raster and patterns adapted from C5VTX.
use core::{ffi::c_char, slice};

#[cfg(feature = "device-bench")]
pub const NTSC: u32 = 0;
pub const PAL: u32 = 1;
pub const W: usize = 256;
pub const H: usize = 240;
pub const BOUNCE: u32 = 5;
pub const PATTERN_COUNT: u32 = 6;

// Matches cvbs_frame_t. C owns the backing allocation.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Frame {
    pub dma: *mut u8,
    samples: u32,
    lines: u32,
    first_row: u32,
    active_x: u32,
    bus_width: u32,
    pub blank: u8,
    black: u8,
    white: u8,
    max_code: u8,
    pub mode: u32,
}

impl Frame {
    #[cfg(feature = "device-bench")]
    pub const EMPTY: Self = Self {
        dma: core::ptr::null_mut(), samples: 0, lines: 0, first_row: 0, active_x: 0,
        bus_width: 0, blank: 0, black: 0, white: 0, max_code: 0, mode: NTSC,
    };

    pub fn new(dma: *mut u8, mode: u32, bus_width: u32) -> Self {
        let (samples, lines, first_row, active_x) =
            if mode == PAL { (640, 312, 47, 104) } else { (636, 262, 21, 100) };
        let (blank, black, white, max_code) = if bus_width == 2 {
            (1, 1, 3, 3)
        } else {
            (18, if mode == PAL { 18 } else { 21 }, 62, 63)
        };
        Self { dma, samples, lines, first_row, active_x, bus_width, blank, black, white, max_code, mode }
    }

    pub fn bytes(&self) -> usize {
        self.lines as usize * self.stride()
    }

    fn stride(&self) -> usize {
        self.samples as usize * self.bus_width as usize / 8
    }

    fn timing_code(&self, line: usize, column: usize) -> u8 {
        let half = self.samples as usize / 2;
        let pulse = line * 2 + usize::from(column >= half);
        let count = if self.mode == PAL { 5 } else { 6 };
        let sync = if pulse < 3 * count {
            let low = if (count..2 * count).contains(&pulse) { half - 47 } else { 23 };
            column % half < low
        } else {
            column < 47
        };
        if sync { 0 } else { self.blank }
    }

    fn sync(&self, buffer: &mut [u8]) {
        for (line, row) in buffer.chunks_exact_mut(self.stride()).enumerate() {
            for (x, byte) in row.iter_mut().enumerate() {
                if self.bus_width == 2 {
                    let x = 4 * x;
                    *byte = self.timing_code(line, x)
                        | self.timing_code(line, x + 1) << 2
                        | self.timing_code(line, x + 2) << 4
                        | self.timing_code(line, x + 3) << 6;
                } else {
                    *byte = self.timing_code(line, x);
                }
            }
        }
    }

    fn sample_offset(&self, x: usize, y: usize) -> usize {
        (self.first_row as usize + y) * self.samples as usize + self.active_x as usize + 2 * x
    }

    fn draw_row(&self, buffer: &mut [u8], y: usize, luma: &[u8; W]) {
        let black = u32::from(self.black);
        let range = u32::from(self.white) - black;
        let code = |gray| (black + (u32::from(gray) * range + 127) / 255) as u8;
        let first = self.sample_offset(0, y);
        if self.bus_width == 2 {
            let row = &mut buffer[first / 4..first / 4 + W / 2];
            for (out, pixels) in row.iter_mut().zip(luma.chunks_exact(2)) {
                *out = code(pixels[0]) * 5 | code(pixels[1]) * 80;
            }
        } else {
            let row = &mut buffer[first..first + 2 * W];
            for (out, &pixel) in row.chunks_exact_mut(2).zip(luma) {
                out.fill(code(pixel));
            }
        }
    }

    pub fn set_levels(&mut self, black: u32, white: u32) -> bool {
        if black < u32::from(self.blank) || white <= black || white > u32::from(self.max_code) {
            return false;
        }
        self.black = black as u8;
        self.white = white as u8;
        true
    }

    // The caller provides initialized storage for the configured raster, with
    // exclusive CPU access. DMA may be reading while active pixels are redrawn.
    pub unsafe fn write_sync(&self) {
        self.sync(slice::from_raw_parts_mut(self.dma, self.bytes()));
    }

    pub unsafe fn write_pattern(&self, pattern: u32, tick: u32) {
        let buffer = slice::from_raw_parts_mut(self.dma, self.bytes());
        let bx = reflect(tick.wrapping_mul(3), W - 24);
        let by = reflect(tick.wrapping_mul(2), H - 48);
        let value = (tick % 1000) as usize;
        for y in 0..H {
            let mut row = [0; W];
            for (x, gray) in row.iter_mut().enumerate() {
                *gray = match pattern {
                    1 => (255 - (x / 32) * 255 / 7) as u8,
                    2 => if x % 32 == 0 || y % 24 == 0 { 255 } else { 0 },
                    3 => ((x / 16) * 17) as u8,
                    4 => if x < W / 2 { 0 } else { 255 },
                    BOUNCE => moving_pixel(x, y, bx, by, value),
                    _ => 0,
                };
            }
            self.draw_row(buffer, y, &row);
        }
    }
}

const DIGITS: [[u8; 7]; 10] = [
    [14,17,19,21,25,17,14], [4,12,4,4,4,4,14], [14,17,1,2,4,8,31],
    [30,1,1,14,1,1,30], [2,6,10,18,31,2,2], [31,16,16,30,1,1,30],
    [14,16,16,30,17,17,14], [31,1,2,4,8,8,8], [14,17,17,14,17,17,14],
    [14,17,17,15,1,1,14],
];

fn reflect(phase: u32, extent: usize) -> usize {
    let phase = phase as usize % (2 * extent);
    phase.min(2 * extent - phase)
}

fn moving_pixel(x: usize, y: usize, bx: usize, by: usize, value: usize) -> u8 {
    if (bx..bx + 24).contains(&x) && (by..by + 24).contains(&y) {
        return 255;
    }
    if (220..234).contains(&y) && (16..52).contains(&x) {
        let slot = (x - 16) / 12;
        let column = ((x - 16) % 12) / 2;
        let digit = value / [100, 10, 1][slot] % 10;
        if column < 5 && DIGITS[digit][(y - 220) / 2] & (16 >> column) != 0 {
            return 255;
        }
    }
    0
}

#[repr(transparent)]
pub struct PatternName(*const c_char);
// These pointers refer only to immutable static strings.
unsafe impl Sync for PatternName {}

#[no_mangle]
pub static pattern_names: [PatternName; PATTERN_COUNT as usize] = [
    PatternName(c"black".as_ptr()), PatternName(c"bars".as_ptr()), PatternName(c"grid".as_ptr()),
    PatternName(c"ramp".as_ptr()), PatternName(c"levels".as_ptr()), PatternName(c"bounce".as_ptr()),
];

// C passes a valid frame and, for rendering, its initialized, non-aliasing
// DMA allocation. cvbs_configure may initialize a previously uninitialized frame.
#[no_mangle]
pub unsafe extern "C" fn cvbs_configure(frame: *mut Frame, dma: *mut u8, mode: u32, width: u32) {
    frame.write(Frame::new(dma, mode, width));
}

#[no_mangle]
pub unsafe extern "C" fn cvbs_bytes(frame: *const Frame) -> usize {
    (*frame).bytes()
}

#[no_mangle]
pub unsafe extern "C" fn cvbs_sync(frame: *mut Frame) {
    (*frame).write_sync();
}

#[no_mangle]
pub unsafe extern "C" fn cvbs_draw_row(frame: *mut Frame, y: u32, luma: *const [u8; W]) {
    let frame = &*frame;
    if (y as usize) < H {
        let buffer = slice::from_raw_parts_mut(frame.dma, frame.bytes());
        frame.draw_row(buffer, y as usize, &*luma);
    }
}

#[no_mangle]
pub unsafe extern "C" fn cvbs_luma_at(frame: *const Frame, x: u32, y: u32) -> u8 {
    let frame = &*frame;
    if x as usize >= W || y as usize >= H {
        return 0;
    }
    let sample = frame.sample_offset(x as usize, y as usize);
    let code = if frame.bus_width == 2 {
        (*frame.dma.add(sample / 4) >> (2 * (sample % 4))) & 3
    } else {
        *frame.dma.add(sample)
    };
    if code <= frame.black {
        0
    } else if code >= frame.white {
        255
    } else {
        (u16::from(code - frame.black) * 255 / u16::from(frame.white - frame.black)) as u8
    }
}

#[no_mangle]
pub unsafe extern "C" fn cvbs_set_levels(frame: *mut Frame, black: u32, white: u32) -> bool {
    (*frame).set_levels(black, white)
}

#[no_mangle]
pub unsafe extern "C" fn pattern_draw(frame: *mut Frame, pattern: u32, tick: u32) {
    (*frame).write_pattern(pattern, tick);
}
