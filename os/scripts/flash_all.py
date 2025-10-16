#!/usr/bin/env python3
# flash_all.py — ZIP-only SD flasher
# Expects exactly one *.img and its matching *.img.sha inside the ZIP.
# Always unmounts before writing, always powers off after verify.
# Whitelist ensures only intended card readers are touched.

DEFAULT_ALLOWED_VENDORS = ["TS-RDF5", "TS-RDF5A"]
DEFAULT_ALLOWED_MODELS  = ["SD_Transcend", "Transcend"]

import os, sys, subprocess, zipfile, hashlib, fcntl, struct, errno, time, re, json
from concurrent.futures import ThreadPoolExecutor, as_completed
from tqdm import tqdm
from colorama import Fore, Style, init as color_init

BS = 1 * 1024 * 1024
MIN_EXPECTED_GB = 2
BLKGETSIZE64 = 0x80081272
BLKROGET     = 0x125e

color_init(autoreset=True)


def parse_env_list(name, default):
    raw = os.environ.get(name)
    if not raw:
        return list(default)
    parts = [item for item in re.split(r"[\s,]+", raw.strip()) if item]
    return parts or list(default)


ALLOWED_VENDORS = parse_env_list("FLASH_ALLOWED_VENDORS", DEFAULT_ALLOWED_VENDORS)
ALLOWED_MODELS = parse_env_list("FLASH_ALLOWED_MODELS", DEFAULT_ALLOWED_MODELS)

# ---------- helpers ----------
def sh(args):
    return subprocess.run(args, text=True, stdout=subprocess.PIPE, check=True).stdout

def root_disk():
    src = sh(["findmnt", "-no", "SOURCE", "/"]).strip()
    if src.startswith("/dev/nvme"):
        return src.split("p")[0]
    if src.startswith("/dev/"):
        while src and src[-1].isdigit():
            src = src[:-1]
    return src

def is_removable(dev):
    name = os.path.basename(dev)
    try:
        with open(f"/sys/block/{name}/removable", "rt") as f:
            return f.read().strip() == "1"
    except FileNotFoundError:
        return False

def dev_size_bytes(dev_path):
    try:
        with open(dev_path, "rb", buffering=0) as f:
            buf = fcntl.ioctl(f.fileno(), BLKGETSIZE64, b"\x00"*8)
            return struct.unpack("Q", buf)[0]
    except OSError as e:
        if e.errno in (errno.ENOMEDIUM, errno.EIO):
            return 0
        raise

def dev_is_readonly(dev_path):
    with open(dev_path, "rb", buffering=0) as f:
        ro = fcntl.ioctl(f.fileno(), BLKROGET, b"\x00")
        return bool(ro[0])

def open_exclusive_w(dev_path):
    fd = os.open(dev_path, os.O_WRONLY | os.O_EXCL | os.O_SYNC)
    fcntl.lockf(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    return fd

def unmount_all(dev):
    out = sh(["lsblk","-ln","-o","NAME,MOUNTPOINT",dev])
    for ln in out.splitlines():
        p = ln.split(None,1)[0]
        part = f"/dev/{p}"
        subprocess.run(["udisksctl","unmount","-b",part], check=False,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["umount", part], check=False,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def get_props(dev):
    info = sh(["udevadm", "info", "--query=property", f"--name={dev}"])
    props = dict(line.split("=", 1) for line in info.splitlines() if "=" in line)
    try:
        cap = dev_size_bytes(dev)
    except OSError:
        cap = 0
    return props, cap, is_removable(dev)

# ---------- devices ----------
def candidate_disks():
    j = json.loads(sh(["lsblk", "-J", "-O"]))
    r = root_disk()
    out = []
    for b in j.get("blockdevices", []):
        if b.get("type") != "disk":
            continue
        dev = f"/dev/{b['name']}"
        if dev == r:
            continue
        props, cap, removable = get_props(dev)
        vendor = props.get("ID_VENDOR", "")
        model  = props.get("ID_MODEL", "")
        if not removable: continue
        if vendor not in ALLOWED_VENDORS or model not in ALLOWED_MODELS: continue
        if cap == 0: continue
        out.append({"dev": dev, "size": b.get("size")})
    return out


def device_size_str(dev):
    try:
        return sh(["lsblk", "-dn", "-o", "SIZE", dev]).strip()
    except subprocess.CalledProcessError:
        return "unknown"


def manual_disks(device_paths):
    r = root_disk()
    out = []
    for raw in device_paths:
        dev = os.path.realpath(raw)
        if not dev.startswith("/dev/"):
            dev = f"/dev/{os.path.basename(dev)}"
        if not os.path.exists(dev):
            sys.exit(Fore.RED + f"Device '{raw}' not found." + Style.RESET_ALL)
        if dev == r:
            sys.exit(Fore.RED + f"Refusing to flash root disk '{dev}'." + Style.RESET_ALL)
        sys_block = f"/sys/block/{os.path.basename(dev)}"
        if not os.path.isdir(sys_block):
            sys.exit(Fore.RED + f"'{dev}' is not a whole-disk block device." + Style.RESET_ALL)
        _, cap, removable = get_props(dev)
        if cap == 0:
            sys.exit(Fore.RED + f"Device '{dev}' has no media present." + Style.RESET_ALL)
        if not removable:
            tqdm.write(Fore.YELLOW + f"Warning: '{dev}' is not reported as removable." + Style.RESET_ALL)
        out.append({"dev": dev, "size": device_size_str(dev)})
    return out

# ---------- checksum ----------
def parse_sha256_text(text, image_name):
    text = text.strip()
    m = re.fullmatch(r"[A-Fa-f0-9]{64}", text)
    if m: return text.lower()
    for line in text.splitlines():
        line = line.strip()
        m = re.match(r"^([A-Fa-f0-9]{64})\s+\*?(.+)$", line)
        if m and os.path.basename(m.group(2)) == os.path.basename(image_name):
            return m.group(1).lower()
    return None

# ---------- image source ----------
def image_source(zip_path):
    with zipfile.ZipFile(zip_path, "r") as zf:
        imgs = [n for n in zf.namelist() if n.lower().endswith(".img") and not n.endswith("/")]
        if len(imgs) != 1:
            sys.exit(Fore.RED + f"ZIP must contain exactly one .img (found {len(imgs)})." + Style.RESET_ALL)
        img_name = imgs[0]
        sha_name = img_name + ".sha256"
        if sha_name not in zf.namelist():
            sys.exit(Fore.RED + f"ZIP must contain matching checksum '{sha_name}'." + Style.RESET_ALL)
        info = zf.getinfo(img_name)
        total = info.file_size
        with zf.open(sha_name, "r") as f:
            text = f.read().decode("utf-8", "ignore")
        expected = parse_sha256_text(text, os.path.basename(img_name))
        if not expected:
            sys.exit(Fore.RED + f"Invalid SHA in '{sha_name}'" + Style.RESET_ALL)
    label = f"{os.path.basename(zip_path)}:{img_name}"
    def factory():
        return zipfile.ZipFile(zip_path, "r").open(img_name, "r")
    return factory, total, label, expected

# ---------- hashing / burn / verify ----------
def sha256_stream(f, total, desc, position):
    h = hashlib.sha256()
    with tqdm(total=total, unit="B", unit_scale=True, dynamic_ncols=True,
              desc=desc, miniters=1, position=position, leave=True) as bar:
        while True:
            chunk = f.read(1024*1024)
            if not chunk: break
            h.update(chunk); bar.update(len(chunk))
    return h.hexdigest()

def hash_image(make_reader, total, pos):
    with make_reader() as fin:
        return sha256_stream(fin, total, "image sha256", pos)

def preflight_or_die(dev, image_bytes):
    unmount_all(dev)
    cap = dev_size_bytes(dev)
    if cap == 0: raise RuntimeError("no medium present")
    if cap < image_bytes: raise RuntimeError("too small")
    if cap < MIN_EXPECTED_GB * 1024**3: raise RuntimeError("capacity suspicious")
    if dev_is_readonly(dev): raise RuntimeError("device is read-only")

def burn(dev, make_reader, total, position):
    preflight_or_die(dev, total)
    fd = open_exclusive_w(dev)
    try:
        with make_reader() as fin, os.fdopen(fd, "wb", buffering=0) as fout, \
             tqdm(total=total, unit="B", unit_scale=True, dynamic_ncols=True,
                  desc=f"{os.path.basename(dev)} write", miniters=1, position=position, leave=True) as bar:
            while True:
                buf = fin.read(BS)
                if not buf: break
                fout.write(buf); bar.update(len(buf))
            os.fsync(fout.fileno())
        return 0
    except Exception as e:
        return e

def verify(dev, expected_hex, total, position):
    with open(dev, "rb", buffering=0) as fdev:
        class Limited:
            def __init__(self,f,remain): self.f,self.remain=f,remain
            def read(self,n):
                if self.remain<=0: return b""
                n=min(n,self.remain)
                data=self.f.read(n); self.remain-=len(data); return data
        dev_hex=sha256_stream(Limited(fdev,total),total,f"{os.path.basename(dev)} verify",position)
    return (dev_hex==expected_hex), dev_hex

def power_off(dev):
    subprocess.run(["udisksctl","power-off","-b",dev], check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["eject",dev], check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

# ---------- main ----------
def main():
    if os.geteuid() != 0:
        sys.exit(Fore.RED + "Run with sudo/root" + Style.RESET_ALL)
    if len(sys.argv) != 2:
        sys.exit("Usage: flash_all.py <zipfile>")

    zipfile_path = sys.argv[1]
    make_reader,total,label,expected=image_source(zipfile_path)

    env_devices = os.environ.get("FLASH_DEVICES") or os.environ.get("FLASH_DEVICE")
    if env_devices:
        device_list = [item for item in re.split(r"[\s,]+", env_devices.strip()) if item]
        disks = manual_disks(device_list)
    else:
        disks = candidate_disks()
    if not disks:
        sys.exit(Fore.RED+"No matching SD readers with media found."+Style.RESET_ALL)

    n=len(disks)
    img_hash_pos=2*n
    img_hex=hash_image(make_reader,total,img_hash_pos)
    tqdm.write(f"{Fore.CYAN}Image: {label}{Style.RESET_ALL}")
    tqdm.write(f"{Fore.CYAN}Image SHA-256: {img_hex}{Style.RESET_ALL}")
    if expected!=img_hex:
        sys.exit(Fore.RED+f"Checksum mismatch!\n expected: {expected}\n computed: {img_hex}"+Style.RESET_ALL)
    tqdm.write(Fore.GREEN+"Checksum OK."+Style.RESET_ALL)

    tqdm.write(f"{Fore.CYAN}Targets:{Style.RESET_ALL}")
    for d in disks: tqdm.write(f"  {Fore.YELLOW}{d['dev']}{Style.RESET_ALL} ({d['size']})")

    results_burn={}
    with ThreadPoolExecutor(max_workers=n) as pool:
        futures={pool.submit(burn,d["dev"],make_reader,total,i):d["dev"] for i,d in enumerate(disks)}
        for fut in as_completed(futures):
            dev=futures[fut]
            try: results_burn[dev]=fut.result()
            except Exception as e: results_burn[dev]=e

    tqdm.write("\nWrite results:")
    written=[]
    for d in disks:
        dev=d["dev"]; r=results_burn.get(dev,"no result")
        ok=(r==0)
        color=Fore.GREEN if ok else Fore.RED
        msg="OK" if ok else f"FAIL → {r}"
        tqdm.write(f" {dev}: {color}{msg}{Style.RESET_ALL}")
        if ok: written.append(dev)
    if not written: sys.exit(1)

    results_verify,dev_hexes={},{}
    with ThreadPoolExecutor(max_workers=len(written)) as pool:
        futures={pool.submit(verify,dev,img_hex,total,n+j):dev for j,dev in enumerate(written)}
        for fut in as_completed(futures):
            dev=futures[fut]
            try: ok,dev_hex=fut.result()
            except Exception as e: ok,dev_hex=False,f"ERROR: {e}"
            results_verify[dev]=ok; dev_hexes[dev]=dev_hex

    tqdm.write("")
    for dev in written:
        hex_=dev_hexes.get(dev,""); ok=results_verify.get(dev)
        color=Fore.GREEN if ok else Fore.RED
        status="OK" if ok else "MISMATCH"
        tqdm.write(f"{dev} SHA-256: {Fore.CYAN}{hex_}{Style.RESET_ALL}  [{color}{status}{Style.RESET_ALL}]")

    tqdm.write("\nVerify results:")
    for dev in written:
        ok=results_verify.get(dev,False)
        color=Fore.GREEN if ok else Fore.RED
        tqdm.write(f" {dev}: {color}{'OK' if ok else 'FAIL'}{Style.RESET_ALL}")

    for dev in written:
        if results_verify.get(dev): power_off(dev)

    ok_count=sum(1 for dev in written if results_verify.get(dev))
    fail_count=len(disks)-ok_count
    if fail_count==0:
        tqdm.write(f"\n{Fore.GREEN}Summary: {ok_count} OK / {fail_count} FAIL{Style.RESET_ALL}")
    else:
        tqdm.write(f"\n{Fore.RED}Summary: {ok_count} OK / {fail_count} FAIL{Style.RESET_ALL}")
    tqdm.write(f"Image SHA-256: {Fore.CYAN}{img_hex}{Style.RESET_ALL}")

    sys.exit(0 if fail_count==0 else 1)

if __name__=="__main__":
    main()