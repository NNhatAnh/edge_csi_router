"""Numerical/validation tests only; no RF accuracy or router claim."""
import copy
import math
import unittest

from single_ap_position import fuse


class PositionTests(unittest.TestCase):
    def setUp(self):
        self.mac = "7a:51:0a:19:23:dc"
        self.measurement = {
            "aoa": dict(peer_mac=self.mac, timestamp_s=100.0, status="success", source="calibrated_csi",
                        calibration_id="synthetic-test-only", geometry_verified=True, ambiguity_resolved=True,
                        reference="router_x_axis", angle_deg=30.0),
            "ftm": dict(peer_mac=self.mac, timestamp_s=100.0, status="success", source="ftm",
                        distance_m=3.0, height_difference_m=0.0),
        }

    def check_blocked(self, measurement):
        result = fuse(measurement, self.mac, now=100.1)
        self.assertFalse(result["valid"])
        self.assertIsNone(result["x_m"])
        self.assertIsNone(result["y_m"])
        self.assertTrue(result["reasons"])

    def test_known_geometry(self):
        result = fuse(self.measurement, self.mac, now=100.1)
        self.assertTrue(result["valid"])
        self.assertAlmostEqual(result["x_m"], 3 * math.sqrt(3) / 2)
        self.assertAlmostEqual(result["y_m"], 1.5)

    def test_slant_range_is_not_horizontal_range(self):
        self.measurement["aoa"]["angle_deg"] = 90.0
        self.measurement["ftm"].update(distance_m=5.0, height_difference_m=3.0)
        result = fuse(self.measurement, self.mac, now=100.1)
        self.assertAlmostEqual(result["x_m"], 0.0)
        self.assertAlmostEqual(result["y_m"], 4.0)

    def test_missing_input_never_creates_position(self):
        for value in (None, {}, [], {"aoa": self.measurement["aoa"]}, {"ftm": self.measurement["ftm"]}):
            with self.subTest(value=value):
                self.check_blocked(value)

    def test_wrong_phone_stale_and_unaligned_measurements(self):
        for field, value in (("peer_mac", "ea:99:22:57:83:6e"), ("timestamp_s", 90.0),
                             ("timestamp_s", 101.0), ("timestamp_s", 99.0)):
            m = copy.deepcopy(self.measurement)
            m["aoa"][field] = value
            with self.subTest(field=field, value=value):
                self.check_blocked(m)

    def test_calibration_geometry_and_direction_are_required(self):
        for field, value in (("calibration_id", ""), ("geometry_verified", False),
                             ("ambiguity_resolved", False), ("reference", "unknown"), ("source", "raw_csi")):
            m = copy.deepcopy(self.measurement)
            m["aoa"][field] = value
            with self.subTest(field=field):
                self.check_blocked(m)

    def test_rssi_and_failed_ranging_rejected(self):
        for field, value in (("source", "rssi"), ("status", "failed"), ("distance_m", -1.0),
                             ("height_difference_m", 4.0), ("height_difference_m", None)):
            m = copy.deepcopy(self.measurement)
            m["ftm"][field] = value
            with self.subTest(field=field):
                self.check_blocked(m)

    def test_nonfinite_and_boolean_values_rejected(self):
        for section, field in (("aoa", "angle_deg"), ("ftm", "distance_m"), ("ftm", "timestamp_s")):
            for value in (float("nan"), float("inf"), True, "3"):
                m = copy.deepcopy(self.measurement)
                m[section][field] = value
                with self.subTest(section=section, field=field, value=value):
                    self.check_blocked(m)


if __name__ == "__main__":
    unittest.main()
