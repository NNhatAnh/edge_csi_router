"""Train MỘT Random Forest vẫy tay / không vẫy (Phase 3, ghi chú dự án) trên TOÀN BỘ dữ liệu.

    .venv/bin/python sensing/train.py [data/raw] [models] [--board 192.168.11.1|192.168.4.1|all] [--features motion_core] [--chains 3]

Đánh giá: leave-one-recording-out (mỗi lượt được dự đoán bởi model không thấy lượt đó; không trộn cửa sổ cùng
lượt), rồi fit model cuối trên toàn bộ cửa sổ để deploy. Khác notebook (1 split, không refit): ít lượt nên
LORO cho ước lượng đỡ nhiễu hơn và model cuối dùng hết dữ liệu. Trọng số cân bằng recording × class,
imputation + bỏ cột gần hằng, RF 96 cây max_depth 4 như notebook. Xuất rf_model.json + csi_rf_model.c + parity.
--board all: gộp mọi router; amp84 và --chains 3 dùng 3 chain đầu nên 11.1 (3) và 4.1 (5) ghép được.
Lớp: "wave" vs "no_wave" (nhãn "empty" gộp vào "no_wave").
"""
import argparse, ctypes, json, shutil, subprocess, sys, time
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import confusion_matrix

sys.path.insert(0, str(Path(__file__).parent))
import features as F

DATASET_LABELS = {"wave", "no_wave", "empty"}


def select_folders(data, bw, board, labels=DATASET_LABELS):
    """Lượt dùng được: không exclude, nhãn dataset, đúng bw và đúng router (không trộn 11.1 với 4.1)."""
    out = []
    for f in sorted(Path(data).glob("*/label.json")):
        lab = json.loads(f.read_text())
        if (not lab.get("exclude") and lab.get("label") in labels and lab.get("bw", 0) == bw
                and board in ("all", lab.get("board", "192.168.11.1"))):
            out.append(f.parent)
    return out


def weights(train):
    counts = train.groupby("recording_id").size()
    classes = train[["recording_id", "y"]].drop_duplicates().y.value_counts()
    x = np.array([1 / (counts[r] * classes[y]) for r, y in zip(train.recording_id, train.y)])
    return x / x.sum() * len(x)


def preprocess(train, test, columns):
    x = train[columns].to_numpy(float)
    t = test[columns].to_numpy(float)
    assert not np.isinf(x).any() and not np.isinf(t).any()
    medians = np.array([np.median(v[np.isfinite(v)]) if np.isfinite(v).any() else 0 for v in x.T])
    x = np.where(np.isnan(x), medians, x)
    t = np.where(np.isnan(t), medians, t)
    keep = np.ptp(x, axis=0) > 1e-10 * np.maximum(1, np.max(abs(x), axis=0))
    assert keep.any()
    state = dict(requested_columns=columns, train_imputation_medians=medians.tolist(),
                 kept_columns=np.array(columns)[keep].tolist(),
                 dropped_near_constant_columns=np.array(columns)[~keep].tolist())
    return x[:, keep], t[:, keep], state


def make_rf(seed=42):
    return RandomForestClassifier(n_estimators=96, max_depth=4, min_samples_leaf=5, max_features=0.7,
                                  random_state=seed, n_jobs=1)


def loro(w, cols, group="recording_id", seed=42):
    """Bỏ ra từng nhóm (lượt / vị trí / phiên), train trên phần còn lại, dự đoán nhóm bị bỏ ra.
    Nhóm mà phần train thiếu một lớp thì bỏ qua."""
    per, preds, truths = [], [], []
    for g in sorted(w[group].unique()):
        tr, te = w[w[group] != g], w[w[group] == g]
        if tr.y.nunique() < 2:
            continue
        xf, xt, _ = preprocess(tr, te, cols)
        rf = make_rf(seed).fit(xf, tr.y, sample_weight=weights(tr))
        p = pd.Series(rf.predict_proba(xt)[:, list(rf.classes_).index("wave")], index=te.index)
        preds += list(np.where(p > 0.5, "wave", "no_wave"))
        truths += list(te.y)
        for rid, pr in p.groupby(te.recording_id):
            per.append((rid, te.y[te.recording_id == rid].iloc[0], round(float(pr.mean()), 3)))
    classes = ["no_wave", "wave"]
    ok = sum((p > 0.5) == (t == "wave") for _, t, p in per)
    return dict(per_recording=per, recordings=len(per), recording_correct=ok,
                recording_accuracy=ok / max(len(per), 1), classes=classes,
                window_accuracy=float(np.mean(np.array(preds) == np.array(truths))) if preds else float("nan"),
                window_confusion=confusion_matrix(truths, preds, labels=classes).tolist() if preds else [])


def export_c(rf, prep, out, raw_x):
    """Notebook mục 21: cây → C (csi_rf_predict) + parity với predict_proba trên host."""
    trees = []
    for est in rf.estimators_:
        t = est.tree_
        v = t.value[:, 0, :].astype(float)
        den = v.sum(axis=1, keepdims=True)
        trees.append(dict(children_left=t.children_left.tolist(), children_right=t.children_right.tolist(),
                          feature=t.feature.tolist(), threshold=t.threshold.tolist(),
                          probabilities=np.divide(v, den, out=np.zeros_like(v), where=den > 0).tolist()))
    (out / "rf_model.json").write_text(json.dumps(dict(
        classes=rf.classes_.tolist(), preprocessing=prep, trees=trees, window=F.WINDOW,
        input_contract="vector requested_columns của một cửa sổ 64 record đã QC (features.py)"), indent=1))
    requested, kept = prep["requested_columns"], prep["kept_columns"]

    def arr(kind, name, values):
        values = np.asarray(values).ravel()
        text = ", ".join(str(int(v)) if kind == "int" else format(float(v), ".17g") for v in values)
        return f"static const {kind} {name}[{values.size}] = {{{text}}};\n"

    n_cls = len(rf.classes_)
    lines = ["#include <math.h>\n", f"#define CSI_RF_N_INPUT {len(requested)}\n", f"#define CSI_RF_N_CLASSES {n_cls}\n",
             "const char *csi_rf_labels[CSI_RF_N_CLASSES] = {" + ", ".join(json.dumps(c) for c in rf.classes_) + "};\n",
             arr("int", "feature_mapping", [requested.index(c) for c in kept]),
             arr("double", "imputation", prep["train_imputation_medians"])]
    for j, t in enumerate(trees):
        for kind, key in [("int", "children_left"), ("int", "children_right"), ("int", "feature"),
                          ("double", "threshold"), ("double", "probabilities")]:
            lines.append(arr(kind, f"t{j}_{key}", t[key]))
    lines.append("int csi_rf_predict(const float *input, double *proba) {\n"
                 "  for (int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]=0;\n")
    for j in range(len(trees)):
        lines.append(f"  {{ int node=0;\n    while(t{j}_children_left[node]>=0) {{ int input_index=feature_mapping[t{j}_feature[node]]; "
                     f"float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; "
                     f"node=((double)x<=t{j}_threshold[node])?t{j}_children_left[node]:t{j}_children_right[node]; }}\n"
                     f"    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t{j}_probabilities[node*CSI_RF_N_CLASSES+c]; }}\n")
    lines.append(f"  for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]/={len(trees)}.0;\n"
                 "  int best=0;for(int c=1;c<CSI_RF_N_CLASSES;++c) if(proba[c]>proba[best]) best=c;return best;\n}\n")
    (out / "csi_rf_model.c").write_text("".join(lines))
    (out / "feature_order.json").write_text(json.dumps(requested, indent=1))
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        return dict(status="unverified", reason="không có cc/gcc")
    so = out / "csi_rf_model.so"
    subprocess.check_call([cc, "-O2", "-shared", "-fPIC", str(out / "csi_rf_model.c"), "-lm", "-o", str(so)])
    fn = ctypes.CDLL(str(so.resolve())).csi_rf_predict
    fn.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_double)]
    fn.restype = ctypes.c_int
    raw_x = np.ascontiguousarray(raw_x.astype(np.float32))
    med = np.array(prep["train_imputation_medians"])
    keep_idx = [requested.index(c) for c in kept]
    py = rf.predict_proba(np.where(np.isnan(raw_x), med, raw_x)[:, keep_idx].astype(np.float32))
    c_prob, c_lab = np.zeros_like(py), []
    for i, row in enumerate(raw_x):
        buf = np.zeros(n_cls)
        c_lab.append(fn(row.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), buf.ctypes.data_as(ctypes.POINTER(ctypes.c_double))))
        c_prob[i] = buf
    so.unlink()
    err = float(np.max(abs(py - c_prob)))
    assert np.array_equal(np.argmax(py, axis=1), c_lab) and err < 1e-12, err
    return dict(status="PASS", windows_checked=len(raw_x), probability_max_error=err, scope="host compiler parity only")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data", nargs="?", default="data/raw")
    ap.add_argument("models", nargs="?", default="models")
    ap.add_argument("--bw", type=int, default=2, help="chỉ dùng lượt thu có bw này (2 = 80 MHz)")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--features", default="amp84", choices=["amp84", "motion", "motion_core", "inv", "inv_nb", "inv_hf", "inv_ratio"],
                    help="amp84 = notebook (64 record không chồng); motion* = TSF, cửa sổ 3.2 s trượt 0.8 s; "
                         "inv = bất biến môi trường (CSI-ratio + tone nhạy); inv_nb = inv chuẩn hoá theo nền (lượt calib); "
                         "inv_hf = dải vẫy 1–4 Hz + tỉ lệ dải nhanh, chuẩn hoá theo nền; inv_ratio = chỉ pha CSI-ratio (chuyển thiết bị tốt nhất)")
    ap.add_argument("--group", default="recording_id", choices=["recording_id", "position", "room", "session", "person", "board"],
                    help="đơn vị bỏ ra khi đánh giá: lượt, hay CẢ vị trí / phòng / phiên / người / router (board: dùng với --board all)")
    ap.add_argument("--board", default="192.168.11.1",
                    help="router (192.168.11.1 / 192.168.4.1) hoặc 'all' = mọi router (dùng --chains 3 chung)")
    ap.add_argument("--chains", type=int, default=3, help="motion*: số chain đầu dùng (11.1 có 3, 4.1 có 5)")
    ap.add_argument("--no-export", action="store_true")
    a = ap.parse_args()

    folders = select_folders(a.data, a.bw, a.board)
    parts, qcs = [], []
    columns = dict(amp84=F.AMP_FEATURES, motion=F.motion_feature_names(a.chains),
                   motion_core=F.motion_core_names(a.chains), inv=F.invariant_names(a.chains),
                   inv_nb=F.baseline_names(a.chains), inv_hf=F.band_names(a.chains), inv_ratio=F.ratio_names(a.chains))[a.features]
    for folder in folders:
        w, qc = F.recording_windows(folder)
        if a.features.startswith("motion"):
            w = F.motion_windows(folder, a.chains)
            qc["windows"] = len(w)
        elif a.features.startswith("inv"):
            w = F.invariant_windows(folder, a.chains)
            qc["windows"] = len(w)
        w["board"] = json.loads((folder / "label.json").read_text()).get("board", "192.168.11.1")
        qcs.append(qc)
        parts.append(w)
    print(pd.DataFrame(qcs).to_string(index=False) if qcs else "không có recording nào")
    win = pd.concat([p for p in parts if len(p)], ignore_index=True) if any(len(p) for p in parts) else pd.DataFrame()
    if win.empty:
        sys.exit(f"Không có cửa sổ nào (board={a.board}, bw={a.bw}, nhãn {sorted(DATASET_LABELS)}, exclude=false).")
    baseline_src = None
    if a.features in ("inv_nb", "inv_hf", "inv_ratio"):  # nền: lượt calib cùng (board, room, session), không dùng để train
        cal = [F.invariant_windows(f, a.chains) for f in select_folders(a.data, a.bw, a.board, {"calib"})]
        cal = pd.concat(cal, ignore_index=True) if cal else None
        win, src = F.apply_baseline(win, F.invariant_names(a.chains), cal)
        baseline_src = src.to_dict("records")
        print("nền theo lượt:\n" + src.to_string(index=False))
        if win.empty:
            sys.exit("Không lượt nào có nền: thu lượt nhãn 'calib' (30–60 s phòng yên) cùng phòng/phiên.")
    win["y"] = np.where(win.label == "wave", "wave", "no_wave")

    rec = win[["recording_id", "y"]].drop_duplicates().sort_values("recording_id")
    counts = rec.y.value_counts()
    if len(counts) < 2 or counts.min() < 2:
        sys.exit(f"Cần ≥ 2 lượt mỗi lớp để đánh giá LORO, hiện có {counts.to_dict()}. Thu thêm rồi chạy lại.")

    # 1) đánh giá: mỗi lượt được dự đoán bởi model KHÔNG thấy lượt đó (leave-one-recording-out)
    ev = loro(win, columns, a.group, a.seed)
    if a.group != "recording_id":
        print(f"đánh giá bỏ-ra-cả-{a.group}: nhóm có {win.groupby(a.group).recording_id.nunique().to_dict()} lượt")
    # 2) model cuối: fit trên TOÀN BỘ cửa sổ (con số ở trên là ước lượng cho model này)
    xf, _, prep = preprocess(win, win.iloc[:0], columns)
    rf = make_rf(a.seed).fit(xf, win.y, sample_weight=weights(win))
    report = dict(
        created=time.strftime("%Y-%m-%dT%H:%M:%S%z"), bw=a.bw, board=a.board,
        chains=a.chains if a.features != "amp84" else 3, group=a.group, classes=rf.classes_.tolist(), feature_set=a.features,
        evaluation=f"leave-one-{a.group}-out; model cuối fit trên toàn bộ dữ liệu", baseline_source=baseline_src,
        recordings=len(rec), windows=len(win), recordings_by_class=counts.to_dict(),
        recordings_by_board=win.groupby("board").recording_id.nunique().to_dict(),
        loro_recording_accuracy=ev["recording_accuracy"], loro_window_accuracy=ev["window_accuracy"],
        loro_window_confusion=ev["window_confusion"], loro_per_recording=ev["per_recording"],
        # kiểm tra nhiễu: nếu gain/AGC khác hẳn giữa 2 lớp, model có thể học radio thay vì chuyển động
        nuisance_by_class=win.groupby("y")[[c for c in ["nuisance_gain_tbl_nonzero_fraction", "nuisance_agc_c0",
                                                        "nuisance_agc_c1"] if c in win]].mean().round(3).to_dict(),
        top_features=sorted(zip(prep["kept_columns"], rf.feature_importances_.round(4).tolist()), key=lambda x: -x[1])[:10],
        qc=qcs, preprocessing=prep,
    )
    out = Path(a.models) / time.strftime("%Y%m%d-%H%M%S")
    out.mkdir(parents=True)
    if not a.no_export:
        report["c_parity"] = export_c(rf, prep, out, win[columns].to_numpy(float))
    (out / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=1, default=str))
    win.drop(columns=columns).to_csv(out / "windows.csv", index=False)
    print(f"\n{len(rec)} lượt {counts.to_dict()}, {len(win)} cửa sổ, router {report['recordings_by_board']}")
    print(f"LORO: {ev['recording_correct']}/{ev['recordings']} lượt đúng, {ev['window_accuracy']:.0%} cửa sổ đúng; "
          f"confusion cửa sổ {ev['classes']}: {ev['window_confusion']}")
    for rid, truth, p in ev["per_recording"]:
        print(f"  {rid:44s} thật={truth:7s} P(wave)={p:.2f} {'✓' if (p > 0.5) == (truth == 'wave') else '✗'}")
    print("nhiễu theo lớp:", report["nuisance_by_class"])
    print("model cuối (toàn bộ dữ liệu), C parity:", report.get("c_parity", {}).get("status", "bỏ qua"), "->", out)


if __name__ == "__main__":
    main()
