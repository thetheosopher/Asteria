"""Convert a legacy astrology chart program's JSON export to Asteria library format."""

import json
import re
import sys
from pathlib import Path


# Standard UTC offsets for timezone abbreviations
TZ_STANDARD_OFFSETS = {
    "EST": -5.0,
    "CST": -6.0,
    "MST": -7.0,
    "PST": -8.0,
    "AHS": -9.0,   # Alaska (post-1983 AKST); legacy "Alaska-Hawaii Standard" was -10
    "IST": 5.5,    # India Standard Time
}

SEX_MAP = {0: "", 1: "F", 2: "M"}


def parse_coord(value: str) -> float:
    """Parse coordinate like '77W46.2' or '072E51' to signed decimal degrees.

    North/East are positive, South/West are negative.
    """
    match = re.match(r"(\d+)([NSEW])(\d+\.?\d*)", value)
    if not match:
        return 0.0
    degrees = int(match.group(1))
    direction = match.group(2)
    minutes = float(match.group(3))
    decimal = degrees + minutes / 60.0
    if direction in ("W", "S"):
        decimal = -decimal
    return round(decimal, 4)


def convert_time_24h(time_str: str, am_pm_flag: bool) -> str:
    """Convert 'H:MM' + AM/PM flag to 'HH:MM' 24-hour format."""
    parts = time_str.split(":")
    hour = int(parts[0])
    minute = parts[1] if len(parts) > 1 else "00"

    if am_pm_flag:  # PM
        if hour != 12:
            hour += 12
    else:  # AM
        if hour == 12:
            hour = 0

    return f"{hour:02d}:{minute}"


def is_noon_default(time_str: str, am_pm_flag: bool) -> bool:
    """Detect the conventional 'noon = unknown time' placeholder."""
    return time_str == "12:00" and am_pm_flag is True


def flip_name(name: str) -> str:
    """Convert 'Last, First Middle' to 'First Middle Last'."""
    if "," not in name:
        return name
    last, first = name.split(",", 1)
    last = last.strip()
    first = first.strip()
    return f"{first} {last}" if first else last


def convert_record(record: dict) -> dict:
    name = record["Name"]
    full_name = flip_name(name)

    birth_date = record["BirthDate"][:10]  # "YYYY-MM-DD"
    time_str = record["BirthTime"]
    am_pm = record["AMPMFlag"]
    noon = is_noon_default(time_str, am_pm)

    lat = parse_coord(record["Latitude"])
    lon = parse_coord(record["Longitude"])

    tz_abbr = record["TimeZone"]
    standard_offset = TZ_STANDARD_OFFSETS.get(tz_abbr, 0.0)
    dst_flag = record["DSTFlag"]

    if dst_flag:
        tz_offset = standard_offset + 1.0
        dst_offset = 1.0
    else:
        tz_offset = standard_offset
        dst_offset = 0.0

    tags = []
    if record["GroupName"]:
        tags.append(record["GroupName"].capitalize())

    notes = record["Comments"] or ""

    birth_event = {
        "birthDate": birth_date,
        "birthTime": None if noon else convert_time_24h(time_str, am_pm),
        "timeAccuracy": "unknown" if noon else "exact",
        "noonDefaultApplied": noon,
        "housesEnabled": not noon,
        "sourceDescription": f"Imported from legacy chart program (Chart #{record['ChartNumber']})",
        "confidenceScore": 0.0 if noon else 0.9,
        "cityInput": record["BirthPlace"],
        "timezoneName": None,
        "dstMode": "",
        "latitudeDeg": lat,
        "longitudeDeg": lon,
        "timezoneOffsetHours": tz_offset,
        "dstOffsetHours": dst_offset,
    }

    return {
        "fullName": full_name,
        "displayName": full_name,
        "gender": SEX_MAP.get(record["Sex"], ""),
        "tags": tags,
        "notes": notes,
        "birthEvents": [birth_event],
    }


def main() -> None:
    src = Path(__file__).resolve().parents[2] / "assets" / "Charts.json"
    dst = src.with_name("Charts-Converted.json")

    with open(src, encoding="utf-8") as f:
        data = json.load(f)

    people = [convert_record(r) for r in data["BirthData"]]

    output = {
        "format": "asteria-library",
        "version": 1,
        "people": people,
    }

    with open(dst, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2, ensure_ascii=False)
        f.write("\n")

    print(f"Converted {len(people)} records -> {dst}")


if __name__ == "__main__":
    main()
