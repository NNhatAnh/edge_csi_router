"""Fuse a measured AoA and FTM range in the router's horizontal coordinate frame.

This is the fusion/input-validation stage, not an implementation of SPRING+'s
CSI angle estimator. Missing calibration or ranging never produces a position.
"""
import argparse
import json
import math
import time
from pathlib import Path


def fuse(measurement, peer_mac, now=None, max_age_s=2.0, max_skew_s=0.5):
    now = time.time() if now is None else now
    result = dict(valid=False, x_m=None, y_m=None, reasons=[], method="aoa_ftm")
    if not isinstance(measurement, dict):
        measurement = {}
    aoa, ftm = measurement.get("aoa"), measurement.get("ftm")
    aoa = aoa if isinstance(aoa, dict) else {}
    ftm = ftm if isinstance(ftm, dict) else {}
    reasons = result["reasons"]

    def number(value):
        return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)

    for name, data in (("AoA", aoa), ("FTM", ftm)):
        if not data:
            reasons.append(f"No {name} measurement")
            continue
        if str(data.get("peer_mac", "")).lower() != peer_mac.lower():
            reasons.append(f"{name} peer mismatch")
        ts = data.get("timestamp_s")
        if not number(ts) or not -0.25 <= now - ts <= max_age_s:
            reasons.append(f"{name} timestamp stale or invalid")
        if data.get("status") != "success":
            reasons.append(f"{name} measurement failed")
    if aoa:
        if aoa.get("source") != "calibrated_csi" or not aoa.get("calibration_id"):
            reasons.append("AoA hardware calibration missing")
        if aoa.get("geometry_verified") is not True:
            reasons.append("Antenna geometry unverified")
        if aoa.get("ambiguity_resolved") is not True:
            reasons.append("AoA direction ambiguous")
        if aoa.get("reference") != "router_x_axis":
            reasons.append("AoA coordinate convention unknown")
        if not number(aoa.get("angle_deg")):
            reasons.append("AoA angle invalid")
    if ftm:
        if ftm.get("source") != "ftm":
            reasons.append("Range is not an FTM measurement")
        if not number(ftm.get("distance_m")) or ftm["distance_m"] <= 0:
            reasons.append("FTM distance invalid")
        if not number(ftm.get("height_difference_m")):
            reasons.append("Phone/router height difference unknown")
    if not reasons:
        if abs(aoa["timestamp_s"] - ftm["timestamp_s"]) > max_skew_s:
            reasons.append("AoA and FTM measurements too far apart")
        distance, height = ftm["distance_m"], ftm["height_difference_m"]
        if abs(height) > distance:
            reasons.append("Height difference exceeds measured range")
    if reasons:
        return result
    horizontal = math.sqrt(max(0.0, distance * distance - height * height))
    angle = math.radians(aoa["angle_deg"])
    result.update(valid=True, x_m=horizontal * math.cos(angle), y_m=horizontal * math.sin(angle),
                  horizontal_distance_m=horizontal, slant_distance_m=distance,
                  angle_deg=aoa["angle_deg"], calibration_id=aoa["calibration_id"])
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("measurement", type=Path, help="JSON containing aoa and ftm objects")
    parser.add_argument("--peer", required=True)
    parser.add_argument("--now", type=float, help="timestamp for offline replay only")
    args = parser.parse_args()
    print(json.dumps(fuse(json.loads(args.measurement.read_text()), args.peer, args.now), indent=2, allow_nan=False))
