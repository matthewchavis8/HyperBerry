use std::env;
use std::fs;
use std::path::PathBuf;

const HGBP_MAGIC: u32 = 0x5042_4748;
const HGBP_VERSION: u16 = 1;
const HGBP_HEADER_SIZE: usize = 4096;
const HGBP_BOOT_PROTOCOL_LINUX_ARM64: u32 = 1;
const HGBP_FLAG_INITRD_PRESENT: u32 = 1 << 0;

const OFF_MAGIC: usize = 0;
const OFF_VERSION: usize = 4;
const OFF_HEADER_SIZE: usize = 6;
const OFF_TOTAL_SIZE: usize = 8;
const OFF_HEADER_CRC32: usize = 16;
const OFF_PAYLOAD_CRC32: usize = 20;
const OFF_BOOT_PROTOCOL: usize = 24;
const OFF_FLAGS: usize = 28;
const OFF_KERNEL_OFFSET: usize = 32;
const OFF_KERNEL_SIZE: usize = 40;
const OFF_DTB_OFFSET: usize = 48;
const OFF_DTB_SIZE: usize = 56;
const OFF_INITRD_OFFSET: usize = 64;
const OFF_INITRD_SIZE: usize = 72;
const OFF_ENTRY_OFFSET: usize = 80;
const OFF_BUILD_ID: usize = 88;
const BUILD_ID_SIZE: usize = 64;

#[derive(Debug, Default, PartialEq, Eq)]
struct Args {
    kernel: PathBuf,
    dtb: PathBuf,
    initrd: Option<PathBuf>,
    out: PathBuf,
    build_id: Option<String>,
}

#[derive(Debug, PartialEq, Eq)]
struct Layout {
    kernel_offset: usize,
    kernel_size: usize,
    dtb_offset: usize,
    dtb_size: usize,
    initrd_offset: usize,
    initrd_size: usize,
    total_size: usize,
    flags: u32,
}

fn align4k(value: usize) -> usize {
    (value + 4095) & !4095
}

fn crc32(bytes: &[u8]) -> u32 {
    let mut crc = 0xFFFF_FFFFu32;
    for byte in bytes {
        crc ^= u32::from(*byte);
        for _ in 0..8 {
            let mask = 0u32.wrapping_sub(crc & 1);
            crc = (crc >> 1) ^ (0xEDB8_8320 & mask);
        }
    }

    !crc
}

fn write_le16(buf: &mut [u8], off: usize, value: u16) {
    buf[off..off + 2].copy_from_slice(&value.to_le_bytes());
}

fn write_le32(buf: &mut [u8], off: usize, value: u32) {
    buf[off..off + 4].copy_from_slice(&value.to_le_bytes());
}

fn write_le64(buf: &mut [u8], off: usize, value: u64) {
    buf[off..off + 8].copy_from_slice(&value.to_le_bytes());
}

fn usage() -> &'static str {
    "usage: mkguestpkg --kernel Image --dtb guest.dtb [--initrd rootfs.cpio.gz] --out boot/guest.hgbp [--build-id text]"
}

fn parse_args<I>(args: I) -> Result<Args, String>
where
    I: IntoIterator<Item = String>,
{
    let mut parsed = Args::default();
    let mut iter = args.into_iter();
    while let Some(arg) = iter.next() {
        let value = |iter: &mut <I as IntoIterator>::IntoIter, name: &str| {
            iter.next()
                .ok_or_else(|| format!("{name} requires a value\n{}", usage()))
        };

        match arg.as_str() {
            "--kernel" => parsed.kernel = PathBuf::from(value(&mut iter, "--kernel")?),
            "--dtb" => parsed.dtb = PathBuf::from(value(&mut iter, "--dtb")?),
            "--initrd" => parsed.initrd = Some(PathBuf::from(value(&mut iter, "--initrd")?)),
            "--out" => parsed.out = PathBuf::from(value(&mut iter, "--out")?),
            "--build-id" => parsed.build_id = Some(value(&mut iter, "--build-id")?),
            "--help" | "-h" => return Err(usage().to_string()),
            other => return Err(format!("unknown argument: {other}\n{}", usage())),
        }
    }

    if parsed.kernel.as_os_str().is_empty() {
        return Err(format!("missing --kernel\n{}", usage()));
    }
    if parsed.dtb.as_os_str().is_empty() {
        return Err(format!("missing --dtb\n{}", usage()));
    }
    if parsed.out.as_os_str().is_empty() {
        return Err(format!("missing --out\n{}", usage()));
    }

    Ok(parsed)
}

fn layout(kernel_size: usize, dtb_size: usize, initrd_size: usize) -> Result<Layout, String> {
    if kernel_size == 0 {
        return Err("kernel must not be empty".to_string());
    }
    if dtb_size == 0 {
        return Err("dtb must not be empty".to_string());
    }

    let kernel_offset = HGBP_HEADER_SIZE;
    let dtb_offset = align4k(
        kernel_offset
            .checked_add(kernel_size)
            .ok_or("kernel layout overflow")?,
    );

    let has_initrd = initrd_size != 0;
    let initrd_offset = if has_initrd {
        align4k(
            dtb_offset
                .checked_add(dtb_size)
                .ok_or("dtb layout overflow")?,
        )
    } else {
        0
    };

    let last_end = if has_initrd {
        initrd_offset
            .checked_add(initrd_size)
            .ok_or("initrd layout overflow")?
    } else {
        dtb_offset
            .checked_add(dtb_size)
            .ok_or("dtb layout overflow")?
    };

    Ok(Layout {
        kernel_offset,
        kernel_size,
        dtb_offset,
        dtb_size,
        initrd_offset,
        initrd_size,
        total_size: align4k(last_end),
        flags: if has_initrd {
            HGBP_FLAG_INITRD_PRESENT
        } else {
            0
        },
    })
}

fn write_build_id(package: &mut [u8], build_id: Option<&str>) -> Result<(), String> {
    let Some(build_id) = build_id else {
        return Ok(());
    };

    let bytes = build_id.as_bytes();
    if bytes.len() >= BUILD_ID_SIZE {
        return Err(format!(
            "build id must be shorter than {BUILD_ID_SIZE} bytes"
        ));
    }

    package[OFF_BUILD_ID..OFF_BUILD_ID + bytes.len()].copy_from_slice(bytes);
    Ok(())
}

fn build_package(
    kernel: &[u8],
    dtb: &[u8],
    initrd: Option<&[u8]>,
    build_id: Option<&str>,
) -> Result<Vec<u8>, String> {
    let initrd_size = initrd.map_or(0, <[u8]>::len);
    let layout = layout(kernel.len(), dtb.len(), initrd_size)?;
    let mut package = vec![0u8; layout.total_size];

    write_le32(&mut package, OFF_MAGIC, HGBP_MAGIC);
    write_le16(&mut package, OFF_VERSION, HGBP_VERSION);
    write_le16(&mut package, OFF_HEADER_SIZE, HGBP_HEADER_SIZE as u16);
    write_le64(&mut package, OFF_TOTAL_SIZE, layout.total_size as u64);
    write_le32(
        &mut package,
        OFF_BOOT_PROTOCOL,
        HGBP_BOOT_PROTOCOL_LINUX_ARM64,
    );
    write_le32(&mut package, OFF_FLAGS, layout.flags);
    write_le64(&mut package, OFF_KERNEL_OFFSET, layout.kernel_offset as u64);
    write_le64(&mut package, OFF_KERNEL_SIZE, layout.kernel_size as u64);
    write_le64(&mut package, OFF_DTB_OFFSET, layout.dtb_offset as u64);
    write_le64(&mut package, OFF_DTB_SIZE, layout.dtb_size as u64);
    write_le64(&mut package, OFF_INITRD_OFFSET, layout.initrd_offset as u64);
    write_le64(&mut package, OFF_INITRD_SIZE, layout.initrd_size as u64);
    write_le64(&mut package, OFF_ENTRY_OFFSET, 0);
    write_build_id(&mut package, build_id)?;

    package[layout.kernel_offset..layout.kernel_offset + layout.kernel_size]
        .copy_from_slice(kernel);
    package[layout.dtb_offset..layout.dtb_offset + layout.dtb_size].copy_from_slice(dtb);
    if let Some(initrd) = initrd {
        package[layout.initrd_offset..layout.initrd_offset + layout.initrd_size]
            .copy_from_slice(initrd);
    }

    let payload_crc = crc32(&package[HGBP_HEADER_SIZE..]);
    write_le32(&mut package, OFF_PAYLOAD_CRC32, payload_crc);
    let header_crc = {
        let mut header = package[..HGBP_HEADER_SIZE].to_vec();
        write_le32(&mut header, OFF_HEADER_CRC32, 0);
        write_le32(&mut header, OFF_PAYLOAD_CRC32, 0);
        crc32(&header)
    };
    write_le32(&mut package, OFF_HEADER_CRC32, header_crc);

    Ok(package)
}

fn run() -> Result<(), String> {
    let args = parse_args(env::args().skip(1))?;
    let kernel = fs::read(&args.kernel)
        .map_err(|err| format!("failed to read {}: {err}", args.kernel.display()))?;
    let dtb = fs::read(&args.dtb)
        .map_err(|err| format!("failed to read {}: {err}", args.dtb.display()))?;
    let initrd = match &args.initrd {
        Some(path) => Some(
            fs::read(path).map_err(|err| format!("failed to read {}: {err}", path.display()))?,
        ),
        None => None,
    };

    let package = build_package(&kernel, &dtb, initrd.as_deref(), args.build_id.as_deref())?;
    fs::write(&args.out, package)
        .map_err(|err| format!("failed to write {}: {err}", args.out.display()))?;

    Ok(())
}

fn main() {
    if let Err(err) = run() {
        eprintln!("{err}");
        std::process::exit(1);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn read_le16(buf: &[u8], off: usize) -> u16 {
        u16::from_le_bytes([buf[off], buf[off + 1]])
    }

    fn read_le32(buf: &[u8], off: usize) -> u32 {
        u32::from_le_bytes([buf[off], buf[off + 1], buf[off + 2], buf[off + 3]])
    }

    fn read_le64(buf: &[u8], off: usize) -> u64 {
        u64::from_le_bytes([
            buf[off],
            buf[off + 1],
            buf[off + 2],
            buf[off + 3],
            buf[off + 4],
            buf[off + 5],
            buf[off + 6],
            buf[off + 7],
        ])
    }

    #[test]
    fn crc32_matches_standard_check_value() {
        assert_eq!(crc32(b"123456789"), 0xCBF4_3926);
    }

    #[test]
    fn layout_with_initrd_is_canonical() {
        let layout = layout(0x1800, 0x800, 0x2800).unwrap();

        assert_eq!(layout.kernel_offset, HGBP_HEADER_SIZE);
        assert_eq!(layout.dtb_offset, 0x3000);
        assert_eq!(layout.initrd_offset, 0x4000);
        assert_eq!(layout.total_size, 0x7000);
        assert_eq!(layout.flags, HGBP_FLAG_INITRD_PRESENT);
    }

    #[test]
    fn layout_without_initrd_zeros_initrd_offset() {
        let layout = layout(0x1800, 0x800, 0).unwrap();

        assert_eq!(layout.initrd_offset, 0);
        assert_eq!(layout.initrd_size, 0);
        assert_eq!(layout.total_size, 0x4000);
        assert_eq!(layout.flags, 0);
    }

    #[test]
    fn package_header_uses_expected_little_endian_fields() {
        let package = build_package(&vec![0x11; 0x1800], &vec![0x22; 0x800], None, None).unwrap();

        assert_eq!(read_le32(&package, OFF_MAGIC), HGBP_MAGIC);
        assert_eq!(read_le16(&package, OFF_VERSION), HGBP_VERSION);
        assert_eq!(
            read_le16(&package, OFF_HEADER_SIZE),
            HGBP_HEADER_SIZE as u16
        );
        assert_eq!(
            read_le32(&package, OFF_BOOT_PROTOCOL),
            HGBP_BOOT_PROTOCOL_LINUX_ARM64
        );
        assert_eq!(
            read_le64(&package, OFF_KERNEL_OFFSET),
            HGBP_HEADER_SIZE as u64
        );
        assert_eq!(read_le64(&package, OFF_KERNEL_SIZE), 0x1800);
        assert_eq!(read_le64(&package, OFF_DTB_OFFSET), 0x3000);
        assert_eq!(read_le64(&package, OFF_DTB_SIZE), 0x800);
        assert_eq!(read_le64(&package, OFF_INITRD_OFFSET), 0);
        assert_eq!(read_le64(&package, OFF_INITRD_SIZE), 0);
    }

    #[test]
    fn package_crc_fields_follow_hyperberry_rules() {
        let package = build_package(
            &vec![0x11; 0x1800],
            &vec![0x22; 0x800],
            Some(&vec![0x33; 0x2800]),
            None,
        )
        .unwrap();

        let header_crc = read_le32(&package, OFF_HEADER_CRC32);
        let payload_crc = read_le32(&package, OFF_PAYLOAD_CRC32);

        let mut header = package[..HGBP_HEADER_SIZE].to_vec();
        write_le32(&mut header, OFF_HEADER_CRC32, 0);
        write_le32(&mut header, OFF_PAYLOAD_CRC32, 0);

        assert_eq!(header_crc, crc32(&header));
        assert_eq!(payload_crc, crc32(&package[HGBP_HEADER_SIZE..]));
    }

    #[test]
    fn package_writes_build_id() {
        let package =
            build_package(&[1, 2, 3, 4], &[5, 6, 7, 8], None, Some("test-build")).unwrap();

        assert_eq!(&package[OFF_BUILD_ID..OFF_BUILD_ID + 10], b"test-build");
        assert_eq!(package[OFF_BUILD_ID + 10], 0);
    }

    #[test]
    fn rejects_empty_required_components() {
        assert!(build_package(&[], &[1], None, None).is_err());
        assert!(build_package(&[1], &[], None, None).is_err());
    }

    #[test]
    fn rejects_oversized_build_id() {
        let build_id = "x".repeat(BUILD_ID_SIZE);
        assert!(build_package(&[1], &[2], None, Some(&build_id)).is_err());
    }

    #[test]
    fn parses_required_args() {
        let args = parse_args([
            "--kernel".to_string(),
            "Image".to_string(),
            "--dtb".to_string(),
            "guest.dtb".to_string(),
            "--out".to_string(),
            "guest.hgbp".to_string(),
        ])
        .unwrap();

        assert_eq!(args.kernel, PathBuf::from("Image"));
        assert_eq!(args.dtb, PathBuf::from("guest.dtb"));
        assert_eq!(args.out, PathBuf::from("guest.hgbp"));
        assert_eq!(args.initrd, None);
    }
}
