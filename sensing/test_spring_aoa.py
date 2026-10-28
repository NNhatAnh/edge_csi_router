"""Numerical algorithm checks, not RF/physical-angle acceptance.

Run: .venv/bin/python -m unittest discover -s sensing -p test_spring_aoa.py
"""
import unittest
import json
from pathlib import Path
import tempfile

import numpy as np

import spring_aoa as S


def synthetic(indices, paths, antennas=3, frequency=5775e6, spacing=None):
    spacing = S.C / frequency / 2 if spacing is None else spacing
    return sum(gain * np.exp(-2j * np.pi * spacing * frequency / S.C
                            * np.arange(antennas)[:, None] * np.sin(np.deg2rad(angle)))
               * np.exp(-2j * np.pi * np.asarray(indices)[None] * 312500 * delay * 1e-9)
               for angle, delay, gain in paths)


class SourceMathTests(unittest.TestCase):
    def test_matlab_hankel_layout(self):
        h = np.array([[1, 2, 3], [10, 20, 30], [100, 200, 300]], complex)
        expected = [[1, 2, 10, 20], [2, 3, 20, 30],
                    [10, 20, 100, 200], [20, 30, 200, 300]]
        np.testing.assert_array_equal(S.format_csi(h, 2, 2), expected)

    def test_calibration_inverts_known_rf_offsets(self):
        h = synthetic(np.arange(24), [(30, 17, 1)])
        a1 = np.array([1, 3, -2])
        a2 = np.tile(np.linspace(-1, 1, 24), (3, 1))
        phi = np.array([0.3, -1.1, 0.8])
        measured = h * 10 ** ((a1[:, None] + a2) / 20) * np.exp(1j * phi[:, None])
        np.testing.assert_allclose(S.calibrate_csi(measured, a1, a2, phi), h, atol=1e-12)
        with self.assertRaises(ValueError):
            S.calibrate_csi(h, np.zeros(4), np.zeros((4,24)), np.zeros(4))

    def test_sanitization_preserves_inter_chain_phase(self):
        indices = np.r_[np.arange(-12, 0), np.arange(1, 13)]
        phase = 0.08 * indices[None] + np.array([0.6, 0.8, 1.0])[:, None]
        h = (1 + np.arange(3)[:, None]) * np.exp(1j * phase)
        clean, (slope, constant) = S.sanitize_csi(h, indices)
        self.assertAlmostEqual(slope, 0.08)
        self.assertAlmostEqual(constant, 0.8)
        np.testing.assert_allclose(clean[1] / clean[0], h[1] / h[0], atol=1e-12)
        np.testing.assert_allclose(abs(clean), abs(h), atol=1e-12)

    def test_matrix_pencil_one_and_two_poles(self):
        indices = np.arange(40)
        self.assertEqual(S.mpm_path_count(synthetic(indices, [(20, 15, 1)])), 1)
        self.assertEqual(S.mpm_path_count(synthetic(indices, [(-30, 0, 0.7), (40, 110, 1)])), 2)

    def test_rap_matches_dense_source_formula_each_iteration(self):
        # Independent dense implementation of MATLAB projector and delay-fastest grid.
        rng = np.random.default_rng(13)
        cfg = S.Config(5775e6, S.C / 5775e6 / 2, tuple(range(8)),
                       delay_points=13, angle_points=11, max_paths=3)
        estimator = S.Estimator(cfg, 3)
        k, j = 2, 6
        qs = np.linalg.qr(rng.normal(size=(k*j, 3)) + 1j*rng.normal(size=(k*j, 3)))[0]
        got, first = estimator.rap_music(qs, k, j)
        columns = []
        coordinates = []
        for angle in estimator.angles:
            a = np.exp(-1j * np.pi * (np.arange(k)-(k-1)/2) * np.sin(np.deg2rad(angle)))
            for delay in estimator.delays:
                d = np.exp(-2j*np.pi*np.arange(j)*312500*delay)
                columns.append(np.outer(d, a).ravel(order="F"))
                coordinates.append((delay*1e9, angle))
        grid = np.column_stack(columns)
        selected = []
        want = []
        for iteration in range(3):
            if selected:
                b = np.column_stack(selected)
                p = np.eye(k*j) - b @ np.linalg.pinv(b.conj().T @ b) @ b.conj().T
            else:
                p = np.eye(k*j)
            aq = p @ grid
            pq = p @ qs
            score = abs(np.sum(aq.conj() * ((pq @ pq.conj().T) @ aq), axis=0)
                        / np.sum(aq.conj() * aq, axis=0))
            if iteration == 0:
                np.testing.assert_allclose(first.ravel(order="F"), score, atol=1e-12)
            index = np.argmax(score)
            want.append([*coordinates[index], score[index]])
            selected.append(grid[:, index])
        np.testing.assert_allclose(got, want, atol=1e-10)

    def test_first_path_clustering_prefers_earlier_equal_variance_cluster(self):
        rng = np.random.default_rng(5)
        earlier = np.column_stack((rng.normal(-60, 0.5, 30), rng.normal(-30, 0.2, 30), np.ones(30)))
        later = np.column_stack((rng.normal(80, 0.5, 30), rng.normal(40, 0.2, 30), np.ones(30)))
        cfg = S.Config(5775e6, 0.026, tuple(range(24)))
        result = S.first_path_aoa(np.r_[earlier, later], cfg)
        self.assertTrue(result["valid"])
        self.assertAlmostEqual(result["angle_deg"], -30, delta=0.5)
        self.assertEqual(len(result["clusters"]), 2)

    def test_bad_inputs_and_no_cluster_are_explicit(self):
        cfg = S.Config(5775e6, 0.026, tuple(range(24)))
        for rows in [np.empty((0, 3)), np.full((4, 3), np.nan), np.array([[0, 0, 0]])]:
            self.assertFalse(S.first_path_aoa(rows, cfg)["valid"])
        for h in [np.zeros((3, 24)), np.full((3, 24), np.nan), np.ones((1, 24))]:
            with self.assertRaises(ValueError):
                S.sanitize_csi(h, cfg.subcarrier_indices)
        with self.assertRaises(ValueError):
            S.Config(5775e6, -0.026, tuple(range(24)))
        with self.assertRaises(ValueError):
            S.Config(5775e6, 0.026, (0, 1, 2, 2, 3, 4))


class PipelineTests(unittest.TestCase):
    def test_noisy_single_path_angles_with_common_timing_and_phase(self):
        rng = np.random.default_rng(17)
        indices = np.arange(32)
        cfg = S.Config(5775e6, S.C / 5775e6 / 2, tuple(indices))
        for angle in [-60, -30, 0, 30, 60]:
            estimator = S.Estimator(cfg, 3)
            rows = []
            # DBSCAN needs recording-level support: eight packets produced a false
            # tiny cluster with the source's fixed five-component subspace. Use
            # the planned >=100-packet run, and retain that short-window limitation.
            for _ in range(100):
                h = synthetic(indices, [(angle, rng.uniform(-20, 20), np.exp(1j*rng.uniform(-2,2)))])
                h += 0.02*(rng.normal(size=h.shape)+1j*rng.normal(size=h.shape))
                result = estimator.estimate_packet(h)
                rows.extend(result["paths"])
                self.assertGreaterEqual(result["mpm_paths"], 1)
            clustered = S.first_path_aoa(rows, cfg)
            self.assertTrue(clustered["valid"], (angle, clustered))
            self.assertAlmostEqual(clustered["angle_deg"], angle, delta=1.8)


class RecordingQCTests(unittest.TestCase):
    def make_recording(self, folder):
        import pandas as pd
        (folder / "export_csi").mkdir()
        (folder / "export").mkdir()
        (folder / "label.json").write_text(json.dumps(dict(exit_code=0,board="192.168.11.1",sta="aa:bb:cc:dd:ee:ff",exclude=True)))
        records, metadata, samples = [], [], []
        for i in range(1,6):
            tones = 246 if i == 5 else 212
            records.append(dict(record=i,phy_ppdu_id=f"0x{i:04x}",chip_tsf_us=i*50000,
                                kernel_ts_s=1000+i,num_chains=3,tones_per_chain=tones,
                                samples_present=tones*3,samples_missing=0))
            metadata.append(dict(phy_ppdu_id=i,chip_tsf_us=i*50000,
                                 wmi_peer_mac="00:00:00:00:00:00" if i==3 else "aa:bb:cc:dd:ee:ff",
                                 wmi_found=i!=4,wmi_capture_ok=i!=4,wmi_bw=2,
                                 wmi_gain_tbl_ch0=0,wmi_gain_tbl_ch1=0,wmi_gain_tbl_ch2=int(i==2),
                                 wmi_center_freq1=5775))
            samples.extend(dict(record=i,chain=c,tone_index=t,i=1,q=0,present=1)
                           for c in range(3) for t in range(tones))
        pd.DataFrame(records).to_csv(folder / "export_csi/records.csv",index=False)
        pd.DataFrame(metadata).to_csv(folder / "export/records_meta.csv",index=False)
        pd.DataFrame(samples).to_csv(folder / "export_csi/csi_iq.csv",index=False)

    def test_wrong_mac_wmi_gain2_and_layout_do_not_enter_estimator(self):
        from spring_run import load_capture
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)
            self.make_recording(folder)
            records,h,qc=load_capture(folder)
            self.assertEqual(records.record.tolist(),[1])
            self.assertEqual(h.shape,(1,3,212))
            self.assertEqual(qc["raw_records"],5)

    def test_missing_iq_refuses_the_recording(self):
        import pandas as pd
        from spring_run import load_capture
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)
            self.make_recording(folder)
            path=folder / "export_csi/csi_iq.csv"
            samples=pd.read_csv(path)
            samples.iloc[1:].to_csv(path,index=False)
            with self.assertRaises(ValueError):
                load_capture(folder)


if __name__ == "__main__":
    unittest.main()
