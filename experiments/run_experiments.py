#!/usr/bin/env python3
"""Коэффициент сжатия и пропускная способность архиватора Хаффмана.

Для каждого файла программа benchmark делает один непромеряемый прогон
с проверкой совпадения после разжатия, затем REPEATS прогонов.
Время считается внутри процесса по CLOCK_MONOTONIC и включает чтение
и запись файлов. Коэффициент — размер архива, делённый на размер исходного
файла. По повторным замерам считаются среднее, выборочное стандартное
отклонение и 95-процентный доверительный интервал Стьюдента.
"""

import array
import io
import math
import random
import statistics
import struct
import subprocess
import sys
import zipfile
from pathlib import Path

from PIL import Image

REPEATS = 10
KIB = 1024

NOMINAL_SIZES = [1 * KIB, 10 * KIB, 100 * KIB, 1024 * KIB, 5 * 1024 * KIB]
SIZE_LABEL = {
    1 * KIB: "1 КБ",
    10 * KIB: "10 КБ",
    100 * KIB: "100 КБ",
    1024 * KIB: "1 МБ",
    5 * 1024 * KIB: "5 МБ",
}
SEED = 20260928
MEGABYTE = 1_000_000

MP4_MIN_BYTES = 30 * KIB

ROOT = Path(__file__).resolve().parent
DATA_DIR = ROOT / "data"
OUT_DIR = ROOT / "out"

TYPE_ORDER = ["ascii", "utf8", "bmp", "wav", "zip", "jpeg", "mp4", "random"]
TYPE_TITLE = {
    "ascii": "ASCII",
    "utf8": "UTF-8",
    "bmp": "BMP",
    "wav": "WAV",
    "zip": "ZIP",
    "jpeg": "JPEG",
    "mp4": "MP4",
    "random": "случайные байты",
}
TYPE_COLOR = {
    "ascii": "#1f77b4",
    "utf8": "#ff7f0e",
    "bmp": "#2ca02c",
    "wav": "#d62728",
    "zip": "#9467bd",
    "jpeg": "#8c564b",
    "mp4": "#e377c2",
    "random": "#7f7f7f",
}

EXPECTED_RATIO = {
    "ascii": (40.0, 55.0),
    "utf8": (50.0, 65.0),
    "bmp": (70.0, 85.0),
    "wav": (70.0, 85.0),
    "zip": (95.0, math.inf),
    "jpeg": (95.0, math.inf),
    "mp4": (95.0, math.inf),
    "random": (95.0, math.inf),
}

STUDENT_T_95 = {
    1: 12.706,
    2: 4.303,
    3: 3.182,
    4: 2.776,
    5: 2.571,
    6: 2.447,
    7: 2.365,
    8: 2.306,
    9: 2.262,
    10: 2.228,
    15: 2.131,
    20: 2.086,
    30: 2.042,
}

ASCII_WEIGHTS = [
    (" ", 1800), ("e", 1270), ("t", 906), ("a", 817), ("o", 750),
    ("i", 697), ("n", 675), ("s", 633), ("h", 609), ("r", 599),
    ("d", 425), ("l", 402), ("c", 278), ("u", 276), ("m", 241),
    ("w", 236), ("f", 223), ("g", 202), ("y", 197), ("p", 193),
    ("b", 149), ("v", 98), ("k", 77), ("j", 15), ("x", 15),
    ("q", 10), ("z", 7), (".", 80), (",", 60), ("\n", 40),
]
UTF8_WEIGHTS = [
    (" ", 1600), ("о", 1097), ("е", 845), ("а", 806), ("и", 736),
    ("н", 670), ("т", 626), ("с", 547), ("р", 473), ("в", 454),
    ("л", 440), ("к", 349), ("м", 321), ("д", 298), ("п", 281),
    ("у", 262), ("я", 201), ("ы", 190), ("ь", 174), ("г", 170),
    ("з", 165), ("б", 159), ("ч", 145), ("й", 121), ("х", 97),
    ("ж", 94), ("ш", 73), ("ю", 64), ("ц", 48), ("щ", 36),
    ("э", 32), ("ф", 26), ("ё", 13), ("ъ", 4), (".", 80),
    (",", 60), ("\n", 40),
]


def weighted_choice(rng, pairs):
    total = sum(weight for _, weight in pairs)
    point = rng.randrange(total)
    for item, weight in pairs:
        if point < weight:
            return item
        point -= weight
    return pairs[-1][0]


def write_ascii(path, target, rng):
    chars = [weighted_choice(rng, ASCII_WEIGHTS) for _ in range(target)]
    path.write_text("".join(chars), encoding="ascii")
    return True


def write_utf8(path, target, rng):
    chunks = []
    size = 0
    while size < target:
        piece = weighted_choice(rng, UTF8_WEIGHTS).encode("utf-8")
        if size + len(piece) > target:
            break
        chunks.append(piece)
        size += len(piece)
    data = b"".join(chunks)
    if len(data) < target:
        data += b" " * (target - len(data))
    path.write_bytes(data)
    return True


def write_bmp(path, target, rng):
    del rng
    # Строка 24-битного BMP кратна 4 байтам, заголовок — 54 байта,
    # поэтому точный целевой размер достижим, только если (размер − 54) делится на 4.
    pixel_budget = target - 54
    pixel_budget -= pixel_budget % 4
    width = 64
    row_stride = (width * 3 + 3) & ~3
    if pixel_budget < row_stride or pixel_budget % row_stride != 0:
        width = 1
        row_stride = 4
    height = pixel_budget // row_stride
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            red = (y + x) % 256
            green = 32
            blue = 40 + (y % 40)
            row += bytes((blue, green, red))
        row += bytes(row_stride - width * 3)
        rows.append(row)
    pixels = b"".join(rows)
    file_size = 54 + len(pixels)
    header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 54)
    info = struct.pack(
        "<IiiHHIIiiII",
        40,
        width,
        height,
        1,
        24,
        0,
        len(pixels),
        2835,
        2835,
        0,
        0,
    )
    path.write_bytes(header + info + pixels)
    return True


def write_wav(path, target, rng):
    del rng
    data_bytes = max(2, (target - 44) & ~1)
    sample_count = data_bytes // 2
    rate = 22050
    frames = array.array("h")
    for index in range(sample_count):
        time = index / rate
        sample = int(
            10000 * math.sin(2 * math.pi * 440 * time)
            + 3000 * math.sin(2 * math.pi * 660 * time)
        )
        frames.append(max(-32768, min(32767, sample)))
    data_size = len(frames) * 2
    header = b"RIFF" + struct.pack("<I", 36 + data_size) + b"WAVE"
    fmt = b"fmt " + struct.pack("<I", 16) + struct.pack("<HHIIHH", 1, 1, rate, rate * 2, 2, 16)
    data = b"data" + struct.pack("<I", data_size)
    path.write_bytes(header + fmt + data + frames.tobytes())
    return True


def zip_bytes(payload):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        archive.writestr("payload.bin", payload)
    return buffer.getvalue()


def write_zip(path, target, rng):
    payload_len = max(1, target - 128)
    best = b""
    best_diff = None
    for _ in range(8):
        data = zip_bytes(rng.randbytes(payload_len))
        diff = len(data) - target
        if best_diff is None or abs(diff) < best_diff:
            best = data
            best_diff = abs(diff)
        if diff == 0:
            break
        payload_len = max(1, payload_len - diff)
    path.write_bytes(best)
    return True


def jpeg_bytes(side, rng):
    image = Image.frombytes("L", (side, side), rng.randbytes(side * side))
    buffer = io.BytesIO()
    image.save(buffer, "JPEG", quality=85)
    return buffer.getvalue()


def write_jpeg(path, target, rng):
    # Шум в JPEG занимает около 0,67 байта на пиксель. Ищем сторону кадра,
    # при которой файл ближе всего к целевому объёму.
    lo = 8
    hi = max(16, int(math.sqrt(target / 0.4)) + 8)
    best = b""
    best_diff = None
    while lo <= hi:
        side = (lo + hi) // 2
        data = jpeg_bytes(side, rng)
        diff = len(data) - target
        if best_diff is None or abs(diff) < best_diff:
            best = data
            best_diff = abs(diff)
        if abs(diff) <= max(64, int(target * 0.02)):
            break
        if diff < 0:
            lo = side + 1
        else:
            hi = side - 1
    path.write_bytes(best)
    return True


def encode_mp4(path, width, height, duration, bitrate_k):
    source = f"nullsrc=s={width}x{height}:r=15:d={duration},noise=alls=100:allf=t"
    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-loglevel",
            "error",
            "-f",
            "lavfi",
            "-i",
            source,
            "-t",
            f"{duration}",
            "-an",
            "-c:v",
            "libx264",
            "-preset",
            "ultrafast",
            "-b:v",
            f"{bitrate_k}k",
            "-pix_fmt",
            "yuv420p",
            str(path),
        ],
        check=True,
    )
    return path.read_bytes()


def write_mp4(path, target, rng):
    del rng
    if target < MP4_MIN_BYTES:
        print(
            f"MP4 на {SIZE_LABEL.get(target, target)} пропущен: контейнер не бывает меньше {MP4_MIN_BYTES} байт",
            file=sys.stderr,
        )
        return False
    if target >= 2 * 1024 * KIB:
        width, height, duration = 640, 480, 1.5
    elif target >= 200 * KIB:
        width, height, duration = 320, 240, 1.0
    else:
        width, height, duration = 160, 120, 0.8
    low, high = 30, 12000
    best = b""
    best_diff = None
    for _ in range(8):
        bitrate = (low + high) // 2
        data = encode_mp4(path, width, height, duration, bitrate)
        diff = len(data) - target
        if best_diff is None or abs(diff) < best_diff:
            best = data
            best_diff = abs(diff)
        if abs(diff) <= max(4096, int(target * 0.05)):
            break
        if diff < 0:
            low = bitrate + 1
        else:
            high = bitrate - 1
        if low > high:
            break
    path.write_bytes(best)
    return True


def write_random(path, target, rng):
    path.write_bytes(rng.randbytes(target))
    return True


EXTENSIONS = {
    "ascii": ".txt",
    "utf8": ".txt",
    "bmp": ".bmp",
    "wav": ".wav",
    "zip": ".zip",
    "jpeg": ".jpg",
    "mp4": ".mp4",
    "random": ".bin",
}

WRITERS = {
    "ascii": write_ascii,
    "utf8": write_utf8,
    "bmp": write_bmp,
    "wav": write_wav,
    "zip": write_zip,
    "jpeg": write_jpeg,
    "mp4": write_mp4,
    "random": write_random,
}


def generate_corpus():
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    files = []
    for type_name in TYPE_ORDER:
        for nominal in NOMINAL_SIZES:
            path = DATA_DIR / f"{type_name}_{nominal}{EXTENSIONS[type_name]}"
            rng = random.Random(SEED + nominal + sum(type_name.encode()))
            if WRITERS[type_name](path, nominal, rng) is False:
                continue
            actual = path.stat().st_size
            files.append((type_name, nominal, path))
            gap = abs(actual - nominal) / nominal
            mark = "" if gap <= 0.05 else "  (дальше 5% от цели)"
            print(f"файл {path.name}: {actual} байт, цель {nominal}{mark}", file=sys.stderr)
    return files


def student_t(degrees):
    if degrees in STUDENT_T_95:
        return STUDENT_T_95[degrees]
    if degrees > 30:
        return 1.960
    lower = max(item for item in STUDENT_T_95 if item <= degrees)
    return STUDENT_T_95[lower]


def confidence_interval(samples):
    center = statistics.fmean(samples)
    if len(samples) < 2:
        return center, 0.0, center, center
    deviation = statistics.stdev(samples)
    half = student_t(len(samples) - 1) * deviation / math.sqrt(len(samples))
    return center, deviation, center - half, center + half


def run_benchmark(benchmark, files):
    runs = []
    for type_name, nominal, path in files:
        name = f"{type_name}_{nominal}"
        print(f"замер {name}", file=sys.stderr)
        completed = subprocess.run(
            [str(benchmark), name, str(path), str(REPEATS)],
            check=True,
            capture_output=True,
            text=True,
        )
        for line in completed.stdout.splitlines():
            label, actual, compressed, compress_s, decompress_s = line.split(",")
            runs.append(
                {
                    "name": label,
                    "type": type_name,
                    "nominal": nominal,
                    "actual": int(actual),
                    "compressed": int(compressed),
                    "compress_s": float(compress_s),
                    "decompress_s": float(decompress_s),
                }
            )
    return runs


def summarize(runs):
    grouped = {}
    for run in runs:
        grouped.setdefault(run["name"], []).append(run)

    rows = []
    for name, group in grouped.items():
        actual = group[0]["actual"]
        compressed = group[0]["compressed"]
        compress_times = [item["compress_s"] for item in group]
        decompress_times = [item["decompress_s"] for item in group]
        compress_mean, compress_sd, compress_low, compress_high = confidence_interval(compress_times)
        decompress_mean, decompress_sd, decompress_low, decompress_high = confidence_interval(decompress_times)
        compress_speeds = [actual / item / MEGABYTE for item in compress_times]
        decompress_speeds = [actual / item / MEGABYTE for item in decompress_times]
        ratio = 100.0 * compressed / actual if actual else math.nan
        rows.append(
            {
                "name": name,
                "type": group[0]["type"],
                "nominal": group[0]["nominal"],
                "actual": actual,
                "compressed": compressed,
                "ratio": ratio,
                "runs": len(group),
                "compress_mean_s": compress_mean,
                "compress_sd_s": compress_sd,
                "compress_ci_low_s": compress_low,
                "compress_ci_high_s": compress_high,
                "decompress_mean_s": decompress_mean,
                "decompress_sd_s": decompress_sd,
                "decompress_ci_low_s": decompress_low,
                "decompress_ci_high_s": decompress_high,
                "compress_MBps": actual / compress_mean / MEGABYTE,
                "decompress_MBps": actual / decompress_mean / MEGABYTE,
                "compress_MBps_sd": statistics.stdev(compress_speeds) if len(compress_speeds) > 1 else 0.0,
                "decompress_MBps_sd": statistics.stdev(decompress_speeds) if len(decompress_speeds) > 1 else 0.0,
            }
        )
    rows.sort(key=lambda row: (TYPE_ORDER.index(row["type"]), row["nominal"]))
    return rows


def write_csv(runs, rows):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    raw_path = OUT_DIR / "runs.csv"
    with raw_path.open("w", encoding="utf-8") as raw_file:
        raw_file.write("name,type,nominal_bytes,actual_bytes,compressed_bytes,compress_s,decompress_s\n")
        for run in runs:
            raw_file.write(
                f"{run['name']},{run['type']},{run['nominal']},{run['actual']},"
                f"{run['compressed']},{run['compress_s']:.9f},{run['decompress_s']:.9f}\n"
            )

    summary_path = OUT_DIR / "summary.csv"
    with summary_path.open("w", encoding="utf-8") as summary_file:
        summary_file.write(
            "type,nominal_bytes,actual_bytes,compressed_bytes,ratio_percent,runs,"
            "compress_mean_s,compress_sd_s,compress_ci95_low_s,compress_ci95_high_s,"
            "decompress_mean_s,decompress_sd_s,decompress_ci95_low_s,decompress_ci95_high_s,"
            "compress_MBps,decompress_MBps\n"
        )
        for row in rows:
            summary_file.write(
                f"{row['type']},{row['nominal']},{row['actual']},{row['compressed']},"
                f"{row['ratio']:.4f},{row['runs']},"
                f"{row['compress_mean_s']:.9f},{row['compress_sd_s']:.9f},"
                f"{row['compress_ci_low_s']:.9f},{row['compress_ci_high_s']:.9f},"
                f"{row['decompress_mean_s']:.9f},{row['decompress_sd_s']:.9f},"
                f"{row['decompress_ci_low_s']:.9f},{row['decompress_ci_high_s']:.9f},"
                f"{row['compress_MBps']:.4f},{row['decompress_MBps']:.4f}\n"
            )


def svg_escape(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def nice_max(value):
    if value <= 0:
        return 1.0
    power = 10 ** math.floor(math.log10(value))
    scaled = value / power
    for step in (1, 2, 2.5, 5, 10):
        if scaled <= step:
            return step * power
    return 10 * power


def write_svg(path, body, width, height):
    path.write_text(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        f"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"{width}\" height=\"{height}\" "
        f"viewBox=\"0 0 {width} {height}\">\n"
        "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n"
        f"{body}</svg>\n",
        encoding="utf-8",
    )


def axis_ticks(top, count=5):
    return [top * index / count for index in range(count + 1)]


def bar_chart(path, title, labels, values, ylabel):
    width, height = 920, 520
    left, right, top, bottom = 80, 30, 60, 90
    plot_width = width - left - right
    plot_height = height - top - bottom
    maximum = nice_max(max(values) * 1.1)
    slot = plot_width / len(values)
    bar_width = slot * 0.62
    parts = [
        f"<text x=\"{width / 2}\" y=\"32\" text-anchor=\"middle\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"18\">{svg_escape(title)}</text>"
    ]
    for tick in axis_ticks(maximum):
        y = top + plot_height - (tick / maximum) * plot_height
        parts.append(f"<line x1=\"{left}\" y1=\"{y:.1f}\" x2=\"{width - right}\" y2=\"{y:.1f}\" stroke=\"#e6e6e6\"/>")
        parts.append(
            f"<text x=\"{left - 8}\" y=\"{y + 4:.1f}\" text-anchor=\"end\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"12\">{tick:.0f}</text>"
        )
    parts.append(f"<line x1=\"{left}\" y1=\"{top}\" x2=\"{left}\" y2=\"{top + plot_height}\" stroke=\"#222\"/>")
    parts.append(
        f"<line x1=\"{left}\" y1=\"{top + plot_height}\" x2=\"{width - right}\" y2=\"{top + plot_height}\" stroke=\"#222\"/>"
    )
    for index, (label, value) in enumerate(zip(labels, values)):
        x = left + index * slot + (slot - bar_width) / 2
        bar_height = 0 if maximum == 0 else value / maximum * plot_height
        y = top + plot_height - bar_height
        parts.append(f"<rect x=\"{x:.1f}\" y=\"{y:.1f}\" width=\"{bar_width:.1f}\" height=\"{bar_height:.1f}\" fill=\"#1f77b4\"/>")
        parts.append(
            f"<text x=\"{x + bar_width / 2:.1f}\" y=\"{y - 6:.1f}\" text-anchor=\"middle\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"12\">{value:.1f}</text>"
        )
        parts.append(
            f"<text x=\"{x + bar_width / 2:.1f}\" y=\"{top + plot_height + 22}\" text-anchor=\"middle\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"12\">{svg_escape(label)}</text>"
        )
    parts.append(
        f"<text x=\"18\" y=\"{top + plot_height / 2}\" transform=\"rotate(-90 18 {top + plot_height / 2})\" text-anchor=\"middle\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"13\">{svg_escape(ylabel)}</text>"
    )
    write_svg(path, "\n".join(parts), width, height)


def decade_label(power):
    superscripts = "⁰¹²³⁴⁵⁶⁷⁸⁹"
    sign = "⁻" if power < 0 else ""
    digits = "".join(superscripts[int(digit)] for digit in str(abs(power)))
    return "10" + sign + digits


def line_chart(path, title, series, ylabel, log_x, log_y=False, xlabel="размер файла, КБ"):
    width, height = 960, 560
    left, right, top, bottom = 80, 210, 60, 70
    plot_width = width - left - right
    plot_height = height - top - bottom
    points = [(x, y) for items in series.values() for x, y in items if x > 0 and y > 0]
    xs = [x for x, _ in points]
    ys = [y for _, y in points]
    xmin, xmax = min(xs), max(xs)
    ymin = min(ys)
    ymax = nice_max(max(ys) * 1.15)
    if log_y:
        ymin = 10 ** math.floor(math.log10(ymin))
        ymax = 10 ** math.ceil(math.log10(max(ys) * 1.05))

    def x_of(value):
        if log_x:
            start, end = math.log10(xmin), math.log10(xmax)
            position = 0.5 if start == end else (math.log10(value) - start) / (end - start)
        else:
            span = xmax - xmin
            position = 0.5 if span == 0 else (value - xmin) / span
        return left + position * plot_width

    def y_of(value):
        if log_y:
            start, end = math.log10(ymin), math.log10(ymax)
            position = 0.5 if start == end else (math.log10(value) - start) / (end - start)
            return top + plot_height - position * plot_height
        return top + plot_height - (value / ymax) * plot_height

    parts = [
        f"<text x=\"{(left + width - right) / 2}\" y=\"32\" text-anchor=\"middle\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"18\">{svg_escape(title)}</text>"
    ]
    if log_y:
        power = int(math.log10(ymin))
        last = int(math.log10(ymax))
        while power <= last:
            tick = 10**power
            y = y_of(tick)
            parts.append(f"<line x1=\"{left}\" y1=\"{y:.1f}\" x2=\"{width - right}\" y2=\"{y:.1f}\" stroke=\"#e6e6e6\"/>")
            parts.append(
                f"<text x=\"{left - 8}\" y=\"{y + 4:.1f}\" text-anchor=\"end\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"12\">{decade_label(power)}</text>"
            )
            power += 1
    else:
        for tick in axis_ticks(ymax):
            y = y_of(tick)
            parts.append(f"<line x1=\"{left}\" y1=\"{y:.1f}\" x2=\"{width - right}\" y2=\"{y:.1f}\" stroke=\"#e6e6e6\"/>")
            parts.append(
                f"<text x=\"{left - 8}\" y=\"{y + 4:.1f}\" text-anchor=\"end\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"12\">{tick:.0f}</text>"
            )
    if log_x:
        power = math.ceil(math.log10(xmin))
        last = math.floor(math.log10(xmax))
        while power <= last:
            tick = 10**power
            x = x_of(tick)
            parts.append(f"<line x1=\"{x:.1f}\" y1=\"{top}\" x2=\"{x:.1f}\" y2=\"{top + plot_height}\" stroke=\"#f2f2f2\"/>")
            parts.append(
                f"<text x=\"{x:.1f}\" y=\"{top + plot_height + 22}\" text-anchor=\"middle\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"12\">{decade_label(power)}</text>"
            )
            power += 1
    parts.append(f"<line x1=\"{left}\" y1=\"{top}\" x2=\"{left}\" y2=\"{top + plot_height}\" stroke=\"#222\"/>")
    parts.append(
        f"<line x1=\"{left}\" y1=\"{top + plot_height}\" x2=\"{width - right}\" y2=\"{top + plot_height}\" stroke=\"#222\"/>"
    )
    for index, (name, items) in enumerate(series.items()):
        color = TYPE_COLOR[name]
        ordered = sorted(items)
        coords = " ".join(f"{x_of(x):.1f},{y_of(y):.1f}" for x, y in ordered)
        parts.append(f"<polyline fill=\"none\" stroke=\"{color}\" stroke-width=\"2\" points=\"{coords}\"/>")
        for x, y in ordered:
            parts.append(f"<circle cx=\"{x_of(x):.1f}\" cy=\"{y_of(y):.1f}\" r=\"3.5\" fill=\"{color}\"/>")
        legend_y = top + 16 + index * 22
        parts.append(f"<rect x=\"{width - right + 24}\" y=\"{legend_y - 10}\" width=\"14\" height=\"14\" fill=\"{color}\"/>")
        parts.append(
            f"<text x=\"{width - right + 46}\" y=\"{legend_y + 2}\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"13\">{svg_escape(TYPE_TITLE[name])}</text>"
        )
    parts.append(
        f"<text x=\"18\" y=\"{top + plot_height / 2}\" transform=\"rotate(-90 18 {top + plot_height / 2})\" text-anchor=\"middle\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"13\">{svg_escape(ylabel)}</text>"
    )
    parts.append(
        f"<text x=\"{(left + width - right) / 2}\" y=\"{height - 16}\" text-anchor=\"middle\" font-family=\"DejaVu Sans, sans-serif\" font-size=\"13\">{svg_escape(xlabel)}</text>"
    )
    write_svg(path, "\n".join(parts), width, height)


def representative(rows, largest):
    chosen = {}
    for row in rows:
        current = chosen.get(row["type"])
        if current is None:
            chosen[row["type"]] = row
            continue
        if largest and row["actual"] > current["actual"]:
            chosen[row["type"]] = row
        if not largest and row["actual"] < current["actual"]:
            chosen[row["type"]] = row
    return [chosen[type_name] for type_name in TYPE_ORDER if type_name in chosen]


def plot(rows):
    for stale in (
        "ratio_large.svg",
        "ratio_small.svg",
        "throughput_compress.svg",
        "throughput_decompress.svg",
    ):
        (OUT_DIR / stale).unlink(missing_ok=True)

    ratio_series = {type_name: [] for type_name in TYPE_ORDER}
    compress_series = {type_name: [] for type_name in TYPE_ORDER}
    decompress_series = {type_name: [] for type_name in TYPE_ORDER}
    for row in rows:
        size_kib = row["actual"] / KIB
        ratio_series[row["type"]].append((size_kib, row["ratio"]))
        compress_series[row["type"]].append((size_kib, row["compress_mean_s"] * 1000))
        decompress_series[row["type"]].append((size_kib, row["decompress_mean_s"] * 1000))
    line_chart(
        OUT_DIR / "ratio_by_size.svg",
        "Коэффициент сжатия и размер файла",
        ratio_series,
        "архив / исходный файл, %",
        True,
    )
    line_chart(
        OUT_DIR / "time_compress.svg",
        "Время сжатия и размер файла",
        compress_series,
        "время, мс",
        True,
        log_y=True,
    )
    line_chart(
        OUT_DIR / "time_decompress.svg",
        "Время разжатия и размер файла",
        decompress_series,
        "время, мс",
        True,
        log_y=True,
    )


def expectation_text(row):
    if row["actual"] < 1024:
        return "файл меньше 1 КБ: возможен рост"
    low, high = EXPECTED_RATIO[row["type"]]
    high_text = "и выше" if math.isinf(high) else f"{high:.0f}%"
    inside = low <= row["ratio"] <= high
    mark = "попадает" if inside else "не попадает"
    return f"ожидание {low:.0f}–{high_text}, замер {mark}"


def format_ms(mean_s, sd_s):
    return f"{mean_s * 1000:.2f} ± {sd_s * 1000:.2f}"


def print_time_table(rows, title, mean_key, sd_key):
    by_type = {type_name: {} for type_name in TYPE_ORDER}
    for row in rows:
        by_type[row["type"]][row["nominal"]] = row
    print(title)
    header = f"{'формат':<18}" + "".join(f"{SIZE_LABEL[size]:>18}" for size in NOMINAL_SIZES)
    print(header)
    for type_name in TYPE_ORDER:
        cells = []
        for size in NOMINAL_SIZES:
            row = by_type[type_name].get(size)
            if row is None:
                cells.append(f"{'—':>18}")
            else:
                cells.append(f"{format_ms(row[mean_key], row[sd_key]):>18}")
        print(f"{TYPE_TITLE[type_name]:<18}" + "".join(cells))
    print()


def print_report(rows):
    print()
    print(f"Повторов на файл: {REPEATS}. Объёмы: 1 КБ, 10 КБ, 100 КБ, 1 МБ, 5 МБ.")
    print_time_table(rows, "Время сжатия, мс (среднее ± ст. отклонение)", "compress_mean_s", "compress_sd_s")
    print_time_table(rows, "Время разжатия, мс (среднее ± ст. отклонение)", "decompress_mean_s", "decompress_sd_s")
    print("Коэффициент сжатия, % (архив / исходный файл)")
    by_type = {type_name: {} for type_name in TYPE_ORDER}
    for row in rows:
        by_type[row["type"]][row["nominal"]] = row
    header = f"{'формат':<18}" + "".join(f"{SIZE_LABEL[size]:>18}" for size in NOMINAL_SIZES)
    print(header)
    for type_name in TYPE_ORDER:
        cells = []
        for size in NOMINAL_SIZES:
            row = by_type[type_name].get(size)
            if row is None:
                cells.append(f"{'—':>18}")
            else:
                cells.append(f"{row['ratio']:>17.1f}%")
        print(f"{TYPE_TITLE[type_name]:<18}" + "".join(cells))
    print()
    print("Крупные файлы и ожидания из обзора:")
    for row in representative(rows, largest=True):
        print(f"  {TYPE_TITLE[row['type']]}: {row['ratio']:.1f}% при {row['actual']} байт — {expectation_text(row)}")


def main():
    benchmark = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT.parent / "build" / "benchmark")
    if not benchmark.is_file():
        print(f"Не найден {benchmark}. Сначала соберите цель benchmark.", file=sys.stderr)
        return 1
    files = generate_corpus()
    runs = run_benchmark(benchmark, files)
    rows = summarize(runs)
    write_csv(runs, rows)
    plot(rows)
    print_report(rows)
    print(f"Таблицы и графики: {OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
