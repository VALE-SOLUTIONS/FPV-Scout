// SPDX-License-Identifier: GPL-3.0-or-later

#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct Shape {
    pub offset_khz: i32,
    pub width_khz: u32,
    pub contrast: u32,
}

/// Find broad, sustained energy in the central 30 MHz of a 40 MS/s FFT.
#[no_mangle]
pub unsafe extern "C" fn scout_rf_shape(bins: *const i16, out: *mut Shape) {
    let Some(out) = out.as_mut() else { return; };
    *out = Shape::default();
    if bins.is_null() { return; }
    let bins = core::slice::from_raw_parts(bins, 256);
    let mut levels = [0i16; 48];
    for (level, chunk) in levels.iter_mut().zip(bins[32..224].chunks_exact(4)) {
        let mut group = [chunk[0], chunk[1], chunk[2], chunk[3]];
        group.sort_unstable();
        *level = group[1];
    }
    // The center of the FFT can contain a receiver DC spur.
    let center = levels[22].min(levels[25]);
    levels[23..25].fill(center);
    let mut sorted = levels;
    sorted.sort_unstable();
    let floor = i32::from(sorted[12]);
    let mut start = 0;
    while start < levels.len() {
        if i32::from(levels[start]) < floor + 1000 {
            start += 1;
            continue;
        }
        let mut end = start + 1;
        while end < levels.len() && i32::from(levels[end]) >= floor + 1000 { end += 1; }
        let width = end - start;
        if width >= 8 {
            let contrast = levels[start..end].iter()
                .map(|&level| i32::from(level) - floor).sum::<i32>() as u32 / width as u32;
            if contrast > out.contrast {
                *out = Shape {
                    offset_khz: ((start + end) as i32 - 48) * 625 / 2,
                    width_khz: width as u32 * 625,
                    contrast,
                };
            }
        }
        start = end;
    }
}

fn iq(word: u32) -> (i32, i32) {
    (((word << 22) as i32) >> 22, ((word << 12) as i32) >> 22)
}

fn coherence(samples: &[u32], start: usize, lag: usize, length: usize) -> u32 {
    let (mut real, mut imag, mut energy) = (0i32, 0i32, 0u32);
    for index in (start..start + length).step_by(4) {
        let (i, q) = iq(samples[index]);
        let (prev_i, prev_q) = iq(samples[index - 1]);
        let (next_i, next_q) = iq(samples[index + lag]);
        let (next_prev_i, next_prev_q) = iq(samples[index + lag - 1]);
        let (a, b) = ((i - prev_i) / 2, (q - prev_q) / 2);
        let (c, d) = ((next_i - next_prev_i) / 2, (next_q - next_prev_q) / 2);
        real += a * c + b * d;
        imag += b * c - a * d;
        energy += (a * a + b * b + c * c + d * d) as u32;
    }
    let (real, imag) = (real.unsigned_abs(), imag.unsigned_abs());
    let magnitude = real.max(imag) + 3 * real.min(imag) / 8;
    // At most 86 terms with signed I/Q differences halved above.
    if energy == 0 { 0 } else { (magnitude * 20 / (energy / 10).max(1)).min(100) }
}

fn matches_timing(samples: &[u32], lag: usize, period: usize, prefix: usize) -> u32 {
    let length = 3 * prefix / 4;
    let needed = 2 * period + lag + length;
    if samples.len() <= needed { return 0; }
    let limit = (samples.len() - needed).min(2 * period);
    for start in (1..limit).step_by(prefix / 5) {
        let mut quality = 100;
        let matched = (0..3).all(|symbol| {
            let at = start + symbol * period;
            let after = at + period / 2;
            let guard = if after + lag + length <= samples.len() { after } else { at - period / 2 };
            let on_prefix = coherence(samples, at, lag, length);
            quality = quality.min(on_prefix);
            on_prefix >= 68 && on_prefix >= 30 + coherence(samples, guard, lag, length)
        });
        if matched { return quality; }
    }
    0
}

#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct Quality {
    pub power: u32,
    pub coherence: u32,
}

/// Look for three symbols whose cyclic prefix repeats, with an off-prefix
/// comparison to reject CW and strongly correlated narrow signals.
#[no_mangle]
pub unsafe extern "C" fn scout_rf_timing(samples: *const u32, n: usize, profiles: u32, quality: *mut Quality) -> u32 {
    let mut quality = quality.as_mut();
    if let Some(value) = quality.as_deref_mut() { *value = Quality::default(); }
    if samples.is_null() || !(4096..=16_380).contains(&n) { return 0; }
    let samples = core::slice::from_raw_parts(samples, n);
    let (mut found, mut best) = (0, 0);
    // O4's reported 30 kHz spacing, and WaveShark's Avatar 2048+256
    // symbols at 22.4 MS/s, expressed in Scout's 40 MS/s samples.
    for (flag, lag, period, prefix) in [(1, 1333, 1427, 94), (2, 3657, 4114, 457), (4, 2667, 2854, 188)] {
        if profiles & flag == 0 { continue; }
        let score = matches_timing(samples, lag, period, prefix);
        if score != 0 {
            found |= flag;
            best = best.max(score);
        }
    }
    if let Some(value) = quality.filter(|_| found != 0) {
        let sum: u32 = samples.iter().step_by(32).map(|&word| {
            let (i, q) = iq(word);
            (i * i + q * q) as u32
        }).sum();
        *value = Quality { power: sum / ((n as u32 + 31) / 32), coherence: best };
    }
    found
}

fn magnitude(i: i32, q: i32) -> u32 {
    let i = i.unsigned_abs();
    let q = q.unsigned_abs();
    i.max(q) + 3 * i.min(q) / 8
}

fn chirp_score(samples: &[u32], start: usize, length: usize, advance: u32, sine: &[i16; 1024]) -> Option<Quality> {
    let (mut iq, mut conjugate_iq, mut straight) = ((0, 0), (0, 0), (0, 0));
    let mut power = 0u32;
    let mut phase = 0u32;
    for n in (start..start + length).step_by(2) {
        let first = samples[n];
        let last = samples[n + 8];
        let (a, b) = ((first as i16 as i32) >> 6, ((first >> 16) as i16 as i32) >> 6);
        let (c, d) = ((last as i16 as i32) >> 6, ((last >> 16) as i16 as i32) >> 6);
        let real = (a * c + b * d) >> 6;
        let imag = (a * d - b * c) >> 6;
        let theta = (phase >> 22) as usize;
        let cos = i32::from(sine[(theta + 256) & 1023]);
        let sin = i32::from(sine[theta]);
        iq.0 += (real * cos + imag * sin) >> 14;
        iq.1 += (imag * cos - real * sin) >> 14;
        conjugate_iq.0 += (real * cos - imag * sin) >> 14;
        conjugate_iq.1 += (imag * cos + real * sin) >> 14;
        straight.0 += real;
        straight.1 += imag;
        power += ((a * a + b * b + c * c + d * d) >> 7) as u32;
        phase = phase.wrapping_add(advance);
    }
    if power < length as u32 * 6 { return None; }
    let chirp = magnitude(iq.0, iq.1).max(magnitude(conjugate_iq.0, conjugate_iq.1));
    let carrier = magnitude(straight.0, straight.1);
    (chirp * 100 >= power * 76 && chirp * 100 >= carrier * 100 + power * 36)
        .then(|| Quality {
            power: power * 64 / ((length as u32 + 1) / 2),
            coherence: (chirp * 100 / power).min(100),
        })
}

/// Recognize the 812.5 kHz LoRa chirp slopes used by 2.4 GHz ELRS.
/// Input is packed signed I/Q, after filtering and decimation to 2 MS/s.
#[no_mangle]
pub unsafe extern "C" fn scout_elrs_chirps(samples: *const u32, n: usize, sine: *const i16, quality: *mut Quality) -> u32 {
    let mut quality = quality.as_mut();
    if let Some(value) = quality.as_deref_mut() { *value = Quality::default(); }
    if samples.is_null() || sine.is_null() || !(100..=1600).contains(&n) { return 0; }
    let samples = core::slice::from_raw_parts(samples, n);
    let sine = &*sine.cast::<[i16; 1024]>();
    for (sf, period, advance) in [
        (5, 79, 354_418_688), (6, 158, 177_209_344), (7, 315, 88_604_672), (8, 630, 44_302_336),
    ] {
        // Eight-sample lag, with the delayed product evaluated every two samples.
        let length = period / 2;
        if n < period + length + 8 { continue; }
        let limit = n - period - length - 8 + 1;
        for start in (0..limit).step_by(period / 5) {
            let Some(first) = chirp_score(samples, start, length, advance, sine) else { continue; };
            let Some(second) = chirp_score(samples, start + period, length, advance, sine) else { continue; };
            if let Some(value) = quality {
                *value = Quality {
                    power: (first.power + second.power) / 2,
                    coherence: first.coherence.min(second.coherence),
                };
            }
            return sf;
        }
    }
    0
}
