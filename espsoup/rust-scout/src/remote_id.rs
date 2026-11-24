// SPDX-License-Identifier: GPL-3.0-or-later
// Wire layouts referenced against github.com/opendroneid/opendroneid-core-c.
const MESSAGE_SIZE: usize = 25;
const ID_SIZE: usize = 20;

fn message(bytes: &[u8], id: &mut [u8; ID_SIZE + 1]) -> bool {
    if bytes.len() != MESSAGE_SIZE || bytes[0] & 15 > 2 || bytes[0] >> 4 > 5 {
        return false;
    }
    if bytes[0] >> 4 == 0 {
        if !(1..=4).contains(&(bytes[1] >> 4)) {
            return false;
        }
        let text = &bytes[2..2 + ID_SIZE];
        let len = text.iter().position(|&c| c == 0).unwrap_or(ID_SIZE);
        if len != 0 && text[..len].iter().all(|c| (b' '..=b'~').contains(c)) && id[0] == 0 {
            id[..len].copy_from_slice(&text[..len]);
        }
    }
    true
}

fn pack(bytes: &[u8], id: &mut [u8; ID_SIZE + 1]) -> bool {
    let [header, size, count, data @ ..] = bytes else { return false };
    if *header >> 4 != 15 || *header & 15 > 2 || *size as usize != MESSAGE_SIZE ||
        !(1..=9).contains(count) || data.len() != usize::from(*count) * MESSAGE_SIZE {
        return false;
    }
    data.chunks_exact(MESSAGE_SIZE).all(|msg| message(msg, id))
}

fn beacon(mut elements: &[u8], id: &mut [u8; ID_SIZE + 1]) -> bool {
    while let [tag, len, tail @ ..] = elements {
        let Some((value, rest)) = tail.split_at_checked(usize::from(*len)) else { return false };
        if *tag == 221 {
            if let [0xfa, 0x0b, 0xbc, 0x0d, _counter, bytes @ ..] = value {
                return pack(bytes, id);
            }
        }
        elements = rest;
    }
    false
}

fn nan(mut attributes: &[u8], id: &mut [u8; ID_SIZE + 1]) -> bool {
    while let [tag, lo, hi, tail @ ..] = attributes {
        let len = u16::from_le_bytes([*lo, *hi]);
        let Some((value, rest)) = tail.split_at_checked(usize::from(len)) else { return false };
        if *tag == 3 {
            if let [0x88, 0x69, 0x19, 0x9d, 0x92, 0x09, _, _, 0x10, info_len, _counter, bytes @ ..] = value {
                return usize::from(*info_len) == 1 + bytes.len() && pack(bytes, id);
            }
        }
        attributes = rest;
    }
    false
}

#[no_mangle]
pub unsafe extern "C" fn rust_scout_rid_wifi(frame: *const u8, len: usize, id: *mut u8) -> u8 {
    if frame.is_null() || id.is_null() || !(24..=2304).contains(&len) {
        return 0;
    }
    let frame = core::slice::from_raw_parts(frame, len);
    let mut text = [0; ID_SIZE + 1];
    if frame[1] & 0xc7 != 0 || frame[22] & 15 != 0 {
        return 0;
    }
    let transport = match (frame[0], &frame[24..]) {
        (0x80, [_, _, _, _, _, _, _, _, _, _, _, _, elements @ ..]) if beacon(elements, &mut text) => 1,
        (0xd0, [0x04, 0x09, 0x50, 0x6f, 0x9a, 0x13, attributes @ ..]) if nan(attributes, &mut text) => 2,
        _ => return 0,
    };
    core::ptr::copy_nonoverlapping(text.as_ptr(), id, text.len());
    transport
}

#[no_mangle]
pub unsafe extern "C" fn rust_scout_rid_ble(data: *const u8, len: usize, id: *mut u8) -> bool {
    if data.is_null() || id.is_null() || !(1..=1650).contains(&len) {
        return false;
    }
    let mut fields = core::slice::from_raw_parts(data, len);
    while let [len, tail @ ..] = fields {
        let Some((field, rest)) = tail.split_at_checked(usize::from(*len)) else { return false };
        if field.is_empty() { break; }
        if let [0x16, 0xfa, 0xff, 0x0d, _counter, payload @ ..] = field {
            let mut text = [0; ID_SIZE + 1];
            let valid = match payload.first() {
                Some(header) if header >> 4 == 15 => pack(payload, &mut text),
                _ => message(payload, &mut text),
            };
            if valid {
                core::ptr::copy_nonoverlapping(text.as_ptr(), id, text.len());
                return true;
            }
        }
        fields = rest;
    }
    false
}
