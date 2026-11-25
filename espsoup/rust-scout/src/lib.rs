// SPDX-License-Identifier: GPL-3.0-or-later
// FPV Scout's detector, confirmation, navigation, spectrum and composite core.
#![no_std]

mod composite;
mod digital;
mod remote_id;
#[cfg(feature = "device-bench")]
mod bench;

#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    // Abort into ESP-IDF's panic path instead of silently hanging the receiver.
    unsafe { abort() }
}
extern "C" { fn abort() -> !; }

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Evidence { h_pairs: u32, v_pairs: u32, polarity: i32 }
#[repr(C)]
#[derive(Default)]
pub struct Scan {
    mode: u32, h_bursts: u32, v_bursts: u32, polarity: i32,
    deadline: i64, last_sync: i64,
}
const _: () = assert!(core::mem::size_of::<Scan>() == 32);
const _: () = assert!(core::mem::size_of::<Evidence>() == 12);

fn sync(v: &[i16], pol: i32) -> Evidence {
    let mut r = Evidence { polarity: pol, ..Evidence::default() };
    let mut lo = 32767;
    let mut hi = -32768;
    for &z in v { let z = pol * i32::from(z); lo = lo.min(z); hi = hi.max(z); }
    if hi - lo < 20 || v.len() < 256 { return r; }
    let mut hist = [0usize; 64];
    for &z in v { hist[((pol * i32::from(z) - lo) * 63 / (hi-lo)) as usize] += 1; }
    let (mut total, mut q03, mut q50, mut got03) = (0, 0, 0, false);
    for (j, count) in hist.iter().enumerate() {
        total += count;
        if !got03 && total >= (v.len()*3+99)/100 { q03=j as i32; got03=true; }
        if total >= v.len()/2 { q50=j as i32; break; }
    }
    let tip=lo+(hi-lo)*q03/63;
    let blank=lo+(hi-lo)*q50/63;
    if blank-tip < 16 { return r; }
    let th=tip+(blank-tip)*40/100;
    let (mut start, mut prev_h, mut prev_v, mut chain) = (-1, -1000, -1000, 0);
    for j in 0..=v.len() {
        let low=j<v.len() && pol*i32::from(v[j])<th;
        if low && start<0 { start=j as i32; }
        if !low && start>=0 {
            let width=j as i32-start;
            if start>0 && j<v.len() {
                if (7..=12).contains(&width) {
                    chain=if (126..=130).contains(&(start-prev_h)) { chain+1 } else { 0 };
                    r.h_pairs=r.h_pairs.max(chain); prev_h=start;
                }
                if (50..=60).contains(&width) {
                    if (62..=66).contains(&(start-prev_v)) { r.v_pairs+=1; }
                    prev_v=start;
                }
            }
            start=-1;
        }
    }
    r
}

// SAFETY: callers provide completed, CPU-owned captures and nonoverlapping buffers.
// Each raw C boundary immediately becomes a bounded slice; all DSP below uses safe Rust.
#[no_mangle]
pub unsafe extern "C" fn rust_fpv_sync(v: *const i16, n: u32, pol: i32) -> Evidence {
    if v.is_null() || n>1638 || (pol!=1 && pol != -1) { return Evidence::default(); }
    sync(core::slice::from_raw_parts(v,n as usize),pol)
}
#[no_mangle]
pub unsafe extern "C" fn rust_fpv_detect(w: *const u32, bins: *mut i16, table: *const u8) -> Evidence {
    if w.is_null() || bins.is_null() || table.is_null() { return Evidence::default(); }
    let w=core::slice::from_raw_parts(w,16380);
    let bins=core::slice::from_raw_parts_mut(bins,1638);
    let table=core::slice::from_raw_parts(table,4096);
    let (mut sum,mut count,mut shift)=(0u32,0u32,0u32);
    for j in (0..w.len()).step_by(61) {
        let mut i=(w[j]&1023) as i32; let mut q=((w[j]>>10)&1023) as i32;
        if i>511 { i-=1024; } if q>511 { q-=1024; }
        sum+=(i.abs()+q.abs()) as u32; count+=1;
    }
    while shift<4 && sum/count>(34<<shift) { shift+=1; }
    let phase=|x:u32| i32::from(table[(((x>>shift)&63)|((x>>(shift+4))&4032)) as usize]);
    let mut prev=phase(w[0]);
    for (b, out) in bins.iter_mut().enumerate() {
        let mut acc=0;
        for k in 0..10 { let p=phase(w[b*10+k]); acc+=((p-prev+128)&255)-128; prev=p; }
        *out=acc as i16;
    }
    let a=sync(bins,1); let b=sync(bins,-1);
    if b.h_pairs+b.v_pairs>a.h_pairs+a.v_pairs { b } else { a }
}
#[no_mangle]
pub unsafe extern "C" fn rust_fpv_resume(s: *mut Scan) { if let Some(s)=s.as_mut() { *s=Scan::default(); } }
#[no_mangle]
pub unsafe extern "C" fn rust_fpv_observe(s: *mut Scan, e: Evidence, now: i64) -> bool {
    let Some(s)=s.as_mut() else { return false; };
    if s.mode==1 && now>=s.deadline { *s=Scan::default(); return false; }
    let h=e.h_pairs>=4;
    if s.mode==2 || s.mode==3 {
        if h && e.polarity==s.polarity { s.last_sync=now; } return false;
    }
    if s.mode==0 {
        if !h { return false; }
        s.mode=1; s.polarity=e.polarity; s.deadline=now.saturating_add(400000);
    }
    if e.polarity==s.polarity {
        if h { s.h_bursts+=1; } if e.v_pairs>=2 { s.v_bursts+=1; }
    }
    if s.h_bursts>=3 && s.v_bursts>0 { s.mode=2; s.last_sync=now; return true; }
    false
}
#[no_mangle]
pub extern "C" fn rust_scout_navigation(page: u32, x: i32, y: i32, landscape: bool) -> i32 {
    let (w, h) = if landscape { (320, 240) } else { (240, 320) };
    if !(0..w).contains(&x) || !(0..h).contains(&y) { return 0; }
    let card = (16..w - 16).contains(&x);
    if page == 0 {
        if !card { return 0; }
        let (stride, height, tool_y, tool_height) = if landscape { (34, 30, 192, 20) } else { (52, 46, 264, 24) };
        let row = y - 55;
        if (0..4 * stride).contains(&row) && row % stride < height {
            return [20, 22, 17, 18][(row / stride) as usize];
        }
        let tool = y - tool_y;
        let tool_stride = tool_height + 4;
        if (0..2 * tool_stride).contains(&tool) && tool % tool_stride < tool_height {
            let second = tool >= tool_stride;
            if x < w / 2 - 6 { return if second { 19 } else { 15 }; }
            if x >= w / 2 + 6 { return if second { 8 } else { 3 }; }
        }
    } else {
        if (8..66).contains(&x) && (6..34).contains(&y) { return 1; }
        if page == 1 && (12..w - 12).contains(&x) && (38..65).contains(&y) {
            return 16;
        }
        if page == 2 && card {
            if (!landscape && (270..302).contains(&y)) ||
               (landscape && x >= w / 2 + 6 && (44..76).contains(&y)) { return 21; }
            if (44..76).contains(&y) && (!landscape || x < w / 2 - 6) { return 14; }
            if (82..114).contains(&y) { return 9; }
            if (118..150).contains(&y) { return 23; }
            if (154..186).contains(&y) {
                if x < w / 2 - 6 { return 11; }
                if x >= w / 2 + 6 { return 10; }
            }
            if (190..222).contains(&y) {
                if !landscape || x < w / 2 - 6 { return 4; }
                if x >= w / 2 + 6 { return 7; }
            }
            if !landscape && (232..264).contains(&y) { return 7; }
        }
        if (page == 1 || page == 3 || page == 4 || page == 7) && (h - 36..h - 4).contains(&y) {
            if (12..w / 2 - 6).contains(&x) { return 5; }
            if (w / 2 + 6..w - 12).contains(&x) {
                if page == 1 {
                    let arrow = if landscape { 32 } else { 24 };
                    if x < w / 2 + 6 + arrow { return 12; }
                    if x < w - 12 - arrow { return 13; }
                }
                return 6;
            }
        }
    }
    0
}
#[no_mangle]
pub extern "C" fn rust_scout_frequency_x(khz:i32)->i32 {
    if khz<=5362000 { return 0; } if khz>=5945000 { return 215; }
    if khz<=5800000 { return ((khz-5362000)*108+438*500)/(438*1000); }
    108+((khz-5800000)*107+145*500)/(145*1000)
}
#[no_mangle]
pub unsafe extern "C" fn rust_scout_signal_score(bins: *const i16) -> i32 {
    if bins.is_null() { return i32::MIN; }
    core::slice::from_raw_parts(bins, 256)
        .iter()
        .enumerate()
        .filter_map(|(i, &power)| {
            let offset = (i as i32 - 128).abs();
            // Ignore DC and favor peaks nearer the tuned frequency.
            ((2..=64).contains(&offset) && power != i16::MIN)
                .then_some(i32::from(power) - 16 * offset)
        })
        .max()
        .unwrap_or(i32::MIN)
}
#[no_mangle]
pub unsafe extern "C" fn rust_scout_spectrum_add(plot:*mut i16,centre:u32,bins:*const i16) {
    if plot.is_null() || bins.is_null() || !(5362..=5970).contains(&centre) { return; }
    let plot=core::slice::from_raw_parts_mut(plot,216);
    let bins=core::slice::from_raw_parts(bins,256);
    for j in 32..224 {
        let khz=centre as i32*1000+(j as i32-128)*40000/256;
        if !(5362000..=5945000).contains(&khz) { continue; }
        let x=rust_scout_frequency_x(khz) as usize;
        plot[x]=plot[x].max(bins[j]);
    }
}
