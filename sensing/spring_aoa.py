"""Offline SPRING+ default AoA/relative-delay algorithm, with explicit RF assumptions.

Translated from S. Eleftherakis et al., SPRING_PLUS, commit
1cea8211cd5a7336074b2d7acd5f88d20c12de13 (IEEE TMC 2024).
Source: https://github.com/stavroseleftherakis/SPRING_PLUS

Implements the default T=1, ULA, signal-subspace RAP-MUSIC path. Calibration
coefficients, antenna coordinates and tone frequencies are inputs, not inferred
from CSI. A numerical cluster is NOT a validated physical AoA measurement.
See docs/SPRING_PLUS_PORT.md for source mapping and deliberate edge-case fixes.
"""
from dataclasses import dataclass

import numpy as np
from scipy.linalg import eigh, svd
from sklearn.cluster import DBSCAN

C = 299792458.0
SOURCE_COMMIT = "1cea8211cd5a7336074b2d7acd5f88d20c12de13"


def _csi(value):
    h = np.asarray(value, dtype=np.complex128)
    if h.ndim != 2 or min(h.shape) < 2 or not np.isfinite(h).all() or not np.any(abs(h)):
        raise ValueError("CSI must be finite, nonzero, antennas x tones, with >=2 of each")
    return h


def calibrate_csi(csi, delta_alpha_1, delta_alpha_2, delta_phi):
    """get_calibratedCSI.m: alpha in dB, phase in radians, input antennas x tones."""
    h = _csi(csi)
    a1 = np.asarray(delta_alpha_1, float).reshape(-1)
    a2 = np.asarray(delta_alpha_2, float)
    phi = np.asarray(delta_phi, float).reshape(-1)
    if a1.shape != (len(h),) or phi.shape != a1.shape or a2.shape != h.shape:
        raise ValueError("Calibration dimensions do not match this antenna/tone layout")
    if not all(np.isfinite(x).all() for x in (a1, a2, phi)):
        raise ValueError("Calibration must be finite")
    return h * 10 ** (-(a1[:, None] + a2) / 20) * np.exp(-1j * phi[:, None])


def sanitize_csi(csi, subcarrier_indices):
    """SpotFi_Sanitization/removePhsSlope: remove ONE common slope and constant.

    Retains relative phase between antennas; this does not calibrate RF chains.
    Returns antennas x tones and the fitted slope/constant in radians.
    """
    h = _csi(csi)
    indices = np.asarray(subcarrier_indices, float)
    if indices.shape != (h.shape[1],) or not np.isfinite(indices).all() or np.ptp(indices) == 0:
        raise ValueError("One finite subcarrier index per tone is required")
    phase = np.unwrap(np.angle(h.T), axis=0)
    for ant in range(h.shape[0]):
        difference = phase[0, ant] - phase[0, 0]
        if difference > np.pi:
            phase[:, ant] -= 2 * np.pi
        elif difference < -np.pi:
            phase[:, ant] += 2 * np.pi
    design = np.column_stack((np.tile(indices, h.shape[0]), np.ones(h.size)))
    slope, constant = np.linalg.lstsq(design, phase.ravel(order="F"), rcond=None)[0]
    return h * np.exp(-1j * (slope * indices + constant))[None], (float(slope), float(constant))


def mpm_path_count(csi, pencil=5, unit_circle_tolerance=0.35):
    """MPM_delayEstimator.m: pole selection and smallest modal count over chains."""
    h = _csi(csi)
    if not 1 <= pencil < h.shape[1] or not 0 < unit_circle_tolerance < 1:
        raise ValueError("Invalid matrix-pencil parameters")
    counts = []
    for chain in h:
        hankel = np.lib.stride_tricks.sliding_window_view(chain, pencil + 1)
        poles = np.linalg.eigvals(np.linalg.pinv(hankel[:, 1:]) @ hankel[:, :-1])
        distance = abs(1 - abs(poles))
        counts.append(max(1, int(np.sum(distance < unit_circle_tolerance))))
    values, freq = np.unique(counts, return_counts=True)
    return int(values[np.argmax(freq)])


def format_csi(csi, k, j):
    """formatCSI.m for T=Ttot=1, preserving MATLAB antenna/Hankel column order."""
    h = _csi(csi)
    m, n = h.shape
    if not 1 <= k <= m or not 1 <= j <= n:
        raise ValueError("Smoothing dimensions exceed the CSI layout")
    windows = np.lib.stride_tricks.sliding_window_view(h, j, axis=1)
    return np.concatenate([
        np.concatenate([windows[a].T for a in range(start, start + m - k + 1)], axis=1)
        for start in range(k)
    ], axis=0)


def signal_subspace(x, n_components=5):
    """GetQnBackscatter.m default main-script branch, which supplies nComps=5.

    Thin SVD gives the same leading subspace as eig(X X^H). When the requested
    subspace includes unobserved directions, use the full covariance eigensystem
    as MATLAB does; its null-space basis is not numerically unique.
    """
    x = np.asarray(x, complex)
    if x.ndim != 2 or not np.isfinite(x).all() or np.linalg.norm(x) == 0:
        raise ValueError("Invalid smoothing matrix")
    if not 1 <= n_components < x.shape[0]:
        raise ValueError("Signal subspace must leave at least one noise dimension")
    if x.shape[1] >= n_components:
        u, s, _ = svd(x, full_matrices=False, check_finite=False)
        return u[:, :n_components], s ** 2
    eigenvalues, u = eigh(x @ x.conj().T, check_finite=False)
    order = np.argsort(abs(eigenvalues))[::-1]
    return u[:, order[:n_components]], abs(eigenvalues[order])


@dataclass(frozen=True)
class Config:
    frequency_hz: float
    spacing_m: float
    subcarrier_indices: tuple
    tone_spacing_hz: float = 312500.0
    delay_range_ns: tuple = (-200.0, 200.0)
    delay_points: int = 201
    angle_range_deg: tuple = (-90.0, 90.0)
    angle_points: int = 101
    max_paths: int = 5
    correlation_threshold: float = 0.8
    pencil: int = 5
    unit_circle_tolerance: float = 0.35
    skip_underdetermined_smoothing: bool = True

    def __post_init__(self):
        indices = np.asarray(self.subcarrier_indices, float)
        numeric = [self.frequency_hz, self.spacing_m, self.tone_spacing_hz,
                   *self.delay_range_ns, *self.angle_range_deg, self.correlation_threshold]
        if not np.isfinite(numeric).all() or min(numeric[:3]) <= 0:
            raise ValueError("Invalid frequency, spacing or grid")
        if indices.ndim != 1 or len(indices) <= self.pencil or not np.isfinite(indices).all() or np.any(np.diff(indices) <= 0):
            raise ValueError("Tone indices must be finite, strictly increasing and longer than pencil")
        if min(self.delay_points, self.angle_points) < 2 or self.max_paths < 1:
            raise ValueError("Grid and path counts must be positive")
        if self.delay_range_ns[0] >= self.delay_range_ns[1] or self.angle_range_deg[0] >= self.angle_range_deg[1]:
            raise ValueError("Invalid grid ranges")
        if self.angle_range_deg != (-90.0, 90.0) or not 0 < self.correlation_threshold < 1:
            raise ValueError("This port uses the source ULA angle domain and a threshold in (0,1)")


def smoothing_schedule(antennas, tones):
    """Source eight (K,J) settings, scaling J and K for a different layout."""
    if antennas < 2:
        raise ValueError("At least two antennas required")
    fractions = np.array([234, 217, 200, 183, 167, 150, 133, 117]) / 234
    js = np.maximum(1, np.rint(tones * fractions).astype(int))
    ks = [antennas] * 4 + [antennas - 1] * 4
    return list(dict.fromkeys(zip(ks, js.tolist())))


class Estimator:
    """Stateful adaptive-smoothing search for one recording; new instance per run."""

    def __init__(self, config, antennas):
        self.config = config
        self.antennas = antennas
        self.indices = np.asarray(config.subcarrier_indices, float)
        self.delays = np.linspace(*config.delay_range_ns, config.delay_points) * 1e-9
        self.angles = np.linspace(*config.angle_range_deg, config.angle_points)
        self.schedule = smoothing_schedule(antennas, len(self.indices))
        self.previous = None

    def _steering(self, k, j, delay_s, angle_deg):
        spatial = np.exp(-2j * np.pi * self.config.spacing_m * self.config.frequency_hz / C
                         * (np.arange(k) - (k - 1) / 2) * np.sin(np.deg2rad(angle_deg)))
        frequency = np.exp(-2j * np.pi * self.indices[:j] * self.config.tone_spacing_hz * delay_s)
        return np.kron(spatial, frequency)

    def rap_music(self, qs, k, j):
        """RAPMusicGridMaxBackscatter default signal projection, T=1.

        Evaluates source a^H P Qs Qs^H P a / a^H P a with separable
        steering, avoiding the huge materialized angle-delay grid matrix.
        Flattening uses delay fastest, matching MATLAB ind2sub/GridPts.
        """
        delay_mat = np.exp(-2j * np.pi * self.indices[:j, None] * self.config.tone_spacing_hz * self.delays)
        spatial_mat = np.exp(-2j * np.pi * self.config.spacing_m * self.config.frequency_hz / C
                             * (np.arange(k)[:, None] - (k - 1) / 2) * np.sin(np.deg2rad(self.angles)))

        def project(basis):
            shaped = basis.reshape(k, j, -1)
            tmp = np.einsum("kjp,jt->pkt", shaped.conj(), delay_mat, optimize=True)
            return np.einsum("pkt,ka->pta", tmp, spatial_mat, optimize=True)

        qsa = project(qs)
        selected, paths = [], []
        first_spectrum = None
        for _ in range(min(self.config.max_paths, qs.shape[1])):
            if selected:
                ahat = np.column_stack(selected)
                projector = ahat @ np.linalg.pinv(ahat.conj().T @ ahat) @ ahat.conj().T
                # QR/SVD basis of the selected column space for low-rank grid projection.
                basis, sing, _ = svd(ahat, full_matrices=False, check_finite=False)
                basis = basis[:, sing > np.finfo(float).eps * max(ahat.shape) * sing[0]]
                ba = project(basis)
                projected = qsa - np.einsum("pu,uta->pta", qs.conj().T @ basis, ba, optimize=True)
                denominator = k * j - np.sum(abs(ba) ** 2, axis=0)
                # Check the low-rank projector against the source formula at selected vectors.
                if not np.allclose(projector, basis @ basis.conj().T, atol=1e-8):
                    raise ArithmeticError("RAP projector became numerically ill-conditioned")
            else:
                projected, denominator = qsa, np.full((len(self.delays), len(self.angles)), k * j, float)
            spectrum = np.divide(np.sum(abs(projected) ** 2, axis=0), denominator,
                                 out=np.full_like(denominator, -np.inf), where=denominator > 1e-9 * k * j)
            if first_spectrum is None:
                first_spectrum = spectrum.copy()
            flat = int(np.argmax(spectrum.ravel(order="F")))
            di, ai = np.unravel_index(flat, spectrum.shape, order="F")
            correlation = float(spectrum[di, ai])
            if not np.isfinite(correlation) or correlation <= 0:
                break
            delay, angle = float(self.delays[di]), float(self.angles[ai])
            paths.append([delay * 1e9, angle, correlation])
            selected.append(self._steering(k, j, delay, angle))
        return np.asarray(paths, float).reshape(-1, 3), first_spectrum

    def estimate_packet(self, csi):
        h = _csi(csi)
        if h.shape != (self.antennas, len(self.indices)):
            raise ValueError("CSI dimensions do not match estimator configuration")
        target = mpm_path_count(h, self.config.pencil, self.config.unit_circle_tolerance)
        sanitized, fit = sanitize_csi(h, self.indices)
        valid = [i for i, (k, j) in enumerate(self.schedule)
                 if k * j > self.config.max_paths and (not self.config.skip_underdetermined_smoothing
                     or (self.antennas - k + 1) * (h.shape[1] - j + 1) >= self.config.max_paths)]
        if not valid:
            raise ValueError("No smoothing setting supports the requested signal subspace")
        choices = valid if self.previous is None else [i for i in valid if abs(i - self.previous) <= 1]
        if not choices:
            choices = valid
        for index in choices:
            k, j = self.schedule[index]
            qs, eigenvalues = signal_subspace(format_csi(sanitized, k, j), self.config.max_paths)
            paths, spectrum = self.rap_music(qs, k, j)
            if not len(paths):
                continue
            count = int(np.sum(paths[:, 2] / paths[0, 2] >= self.config.correlation_threshold))
            delays = paths[:count, 0]
            if count > 1:
                count -= int(np.sum(abs(np.diff(delays)) <= 10))
            result = dict(paths=paths, mpm_paths=target, detected_paths=count, smoothing_index=index,
                          k=k, j=j, sanitization_fit=fit, spectrum=spectrum,
                          signal_eigenvalues=eigenvalues[:self.config.max_paths])
            # Fix source r==numel(search): compare the last searched element, not its value.
            if count > target or index == choices[-1]:
                self.previous = index
                return result
        raise ValueError("No finite path estimates")


def first_path_aoa(path_rows, config):
    """FirstPath_AoAestimator.m DBSCAN/likelihood; returns explicit invalid cases.

    Rows are all paths of packet 0, then all paths of packet 1, etc. Corr is
    normalized to the FIRST row, as in the source (not per packet here).
    Delay is relative after common-slope sanitization, never an FTM range.
    """
    rows = np.asarray(path_rows, float)
    invalid = dict(valid=False, angle_deg=None, clusters=[], reason="no finite paths")
    if rows.ndim != 2 or rows.shape[1] != 3:
        return invalid
    rows = rows[np.isfinite(rows).all(axis=1)]
    if not len(rows) or rows[0, 2] <= 0:
        return invalid
    rows = rows[rows[:, 2] / rows[0, 2] > config.correlation_threshold]
    if not len(rows):
        return {**invalid, "reason": "no paths above correlation threshold"}
    tof = rows[:, 0] + abs(config.delay_range_ns[0])
    if tof.max() <= 0:
        return {**invalid, "reason": "delay normalization is undefined"}
    features = np.column_stack((tof / tof.max(), rows[:, 1] / 90))
    labels = DBSCAN(eps=0.07, min_samples=max(1, int(np.floor(0.07 * len(rows))))).fit_predict(features)
    clusters = []
    for label in sorted(set(labels) - {-1}):
        points = features[labels == label]
        fraction = len(points) / len(features)
        variance = np.var(points, axis=0, ddof=1) if len(points) > 1 else np.zeros(2)
        mean_tof, mean_aoa = np.mean(points, axis=0)
        log_likelihood = fraction - 100 * variance.sum() - mean_tof
        clusters.append(dict(id=int(label), points=len(points), fraction=float(fraction),
                             angle_deg=float(mean_aoa * 90), normalized_delay=float(mean_tof),
                             log_likelihood=float(log_likelihood)))
    if not clusters:
        return {**invalid, "reason": "DBSCAN found only outliers"}
    # Source accepts a sole cluster, even if it occupies <10% of all retained points.
    eligible = clusters if len(clusters) == 1 else [x for x in clusters if x["fraction"] >= 0.1]
    if not eligible:
        return {**invalid, "clusters": clusters, "reason": "all clusters below minimum fraction"}
    best = max(x["log_likelihood"] for x in eligible)
    winners = [x for x in eligible if x["log_likelihood"] == best]
    if len(winners) != 1:
        return {**invalid, "clusters": clusters, "reason": "tied first-path clusters"}
    return dict(valid=True, angle_deg=winners[0]["angle_deg"], clusters=clusters, reason=None)
