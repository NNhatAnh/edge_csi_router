/* wifi_sensing — daemon phát hiện vẫy tay bằng CSI trên router (ghi chú dự án "Quyết định về service").
 *
 * Live:   wifi_sensing [-i phy00.2-0] [-H ath12k_hw2] [-s MAC|auto] [-a IP] [-p 50] [-k 5]
 *                      [-t 0.7] [-T 0.4] [-n 2] [-C 5] [-x 'lệnh khi phát hiện'] [-b baseline.txt]
 * Hiệu chỉnh: wifi_sensing ... -b baseline.txt --calibrate 30   (phòng YÊN → ghi nền = phân vị 20 % feature)
 * Replay: wifi_sensing --replay relay.bin [-b baseline.txt]  (in feature + P(wave) từng cửa sổ, so với Python)
 *
 * Luồng: bật enable_cfr, đọc thẳng kênh relay debugfs (không dùng cfr_test_app: nó ghi /tmp không giới hạn),
 * request CFR định kỳ tới station, bắn UDP keep-awake, cửa sổ 3.2 s trượt 0.8 s theo TSF, 18 feature bất biến
 * môi trường (port của features.invariant_features: biên độ chuẩn hoá, tone nhạy, CSI-ratio giữa anten),
 * chuẩn hoá theo nền nb = log(f / nền) nếu model cần, RF sinh bởi sensing/train.py (model.c), hysteresis,
 * chạy lệnh action. SIGTERM/SIGINT: luôn trả request + enable_cfr về 0.
 */
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "model.c"       /* csi_rf_predict, csi_rf_labels, CSI_RF_N_INPUT — sinh bởi sensing/train.py */
#include "feature_map.h" /* FMAP[CSI_RF_N_INPUT], NEEDS_BASELINE — sinh bởi Makefile từ feature_order.json */

#define TONES 212
#define NONDC 208
#define MAXCH 3            /* 3 chain đầu (khớp --chains 3 khi train) */
#define NF (7 * MAXCH + 5 * (MAXCH - 1)) /* = len(features.invariant_names(3)) */
#define PC 7 /* feature mỗi chain */
#define PP 5 /* feature mỗi cặp CSI-ratio */
#define FS 20.0
#define WIN_S 3.2
#define HOP_S 0.8
#define GRID 64            /* WIN_S * FS */
#define MAX_GAP_US 300000  /* như features.TIME_MAX_GAP_US (cửa sổ thời gian; 300 ms từ 2026-10-20) */
#define PERIOD_NOM 0.0512
#define MIN_COV 0.7
#define RING 256
#define EPS 1e-09
#define MAX_REC 16384      /* 5 chain × 212 tone × 4 B + header ≈ 4.4 KB; lớn hơn = bản ghi hỏng */

typedef struct { double t; int16_t iq[MAXCH][NONDC][2]; } rec_t;

static double base[NF];
static int have_base;

static rec_t ring[RING];
static int ring_n, ring_head; /* ring_head = vị trí ghi tiếp theo */
static volatile sig_atomic_t stop_flag;

/* ---------------------------------------------------------------- relay parse */
static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t rd64(const uint8_t *p) { return rd32(p) | (uint64_t)rd32(p + 4) << 32; }

typedef struct { uint64_t tsf; uint16_t ppdu; int chains, pkt_bw; const int16_t *iq; } csi_t;

/* Trả số byte đã dùng (0 = chưa đủ dữ liệu). *got = 1 nếu là bản ghi CSI hợp lệ. */
static size_t parse_one(const uint8_t *b, size_t len, csi_t *c, int *got)
{
    *got = 0;
    if (len < 4) return 0;
    uint32_t magic = rd32(b);
    if (magic == 0xDEADBEAF) {
        size_t j = 22 + 93;
        if (len < j + 90) return 0;
        const uint8_t *h = b + j;
        size_t hdr = (size_t)h[7] * 4, n = hdr + rd32(h + 8);
        if (rd32(h) != 0xC0DE00BA || n > MAX_REC) return 1; /* bản ghi hỏng (độ dài vô lý): dò tiếp từng byte, không chờ dữ liệu */
        if (len < j + n + 4) return 0;
        if (rd32(h + n) != 0xBEAFDEAD) return 1; /* thiếu magic kết thúc (bản ghi cụt): dò tiếp, như cfr_export.parse_relay */
        uint32_t w5 = rd32(h + 18);
        if (rd32(h) == 0xC0DE00BA && hdr >= 90) {
            c->tsf = rd64(h + 82);
            c->ppdu = h[16] | h[17] << 8;
            c->chains = w5 & 0xFF;
            c->pkt_bw = (w5 >> 24) & 0xFF;
            c->iq = (const int16_t *)(h + hdr);
            *got = (n - hdr) >= (size_t)c->chains * TONES * 4;
        }
        return j + n + 4;
    }
    if (magic == 0xCF4E7A01 || magic == 0xCF4E7A02) {
        if (len < 20) return 0;
        size_t n = 20 + rd32(b + 16) + 4;
        if (n > MAX_REC) return 1;
        return len < n ? 0 : n;
    }
    return 1; /* không phải đầu bản ghi (padding sub-buffer relay, bản ghi KHÔNG căn 4 byte): dò từng byte */
}

/* ---------------------------------------------------------------- features (port features.motion_features) */
static double hann[GRID], twc[33][GRID], tws[33][GRID];

static void init_tables(void)
{
    for (int k = 0; k < GRID; k++) hann[k] = 0.5 - 0.5 * cos(2 * M_PI * k / (GRID - 1));
    for (int f = 0; f < 33; f++)
        for (int k = 0; k < GRID; k++) { twc[f][k] = cos(2 * M_PI * f * k / GRID); tws[f][k] = sin(2 * M_PI * f * k / GRID); }
}

static const char *FNAMES[NF] = {
    "inv_std_c0", "inv_step_c0", "inv_tone_p90_c0", "inv_tone_conc_c0", "inv_hf_c0", "inv_hfrac_c0", "inv_ffrac_c0",
    "inv_std_c1", "inv_step_c1", "inv_tone_p90_c1", "inv_tone_conc_c1", "inv_hf_c1", "inv_hfrac_c1", "inv_ffrac_c1",
    "inv_std_c2", "inv_step_c2", "inv_tone_p90_c2", "inv_tone_conc_c2", "inv_hf_c2", "inv_hfrac_c2", "inv_ffrac_c2",
    "inv_ratio_step_01", "inv_ratio_std_01", "inv_ratio_amp_01", "inv_ratio_hf_01", "inv_ratio_hfrac_01",
    "inv_ratio_step_02", "inv_ratio_std_02", "inv_ratio_amp_02", "inv_ratio_hf_02", "inv_ratio_hfrac_02"};

/* features._band_powers: |rfft(d·hann)|² cộng dồn bin dải vẫy (1–4 Hz: bin 4–12), dải thở (0.1–0.7 Hz: bin 1–2),
 * dải nhanh (4–10 Hz: bin 13–31) và tổng (bin 1–32) */
static void band_pow(const double *d, double *pw, double *pb, double *pf, double *pt)
{
    for (int f = 1; f <= 32; f++) {
        double re = 0, im = 0;
        for (int k = 0; k < GRID; k++) { double x = d[k] * hann[k]; re += x * twc[f][k]; im -= x * tws[f][k]; }
        double pp = re * re + im * im;
        *pt += pp;
        if (f <= 2) *pb += pp; else if (f >= 4 && f <= 12) *pw += pp; else if (f >= 13 && f <= 31) *pf += pp;
    }
}

static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }

/* như np.percentile(x, pct) (nội suy tuyến tính); x bị sắp xếp tại chỗ */
static double percentile(double *x, int n, double pct)
{
    qsort(x, n, sizeof *x, cmp_d);
    double pos = pct / 100.0 * (n - 1);
    int i = (int)pos;
    return i + 1 < n ? x[i] + (pos - i) * (x[i + 1] - x[i]) : x[n - 1];
}

/* Record + trọng số nội suy cho từng điểm lưới (như np.interp: ngoài khoảng giữ giá trị biên). */
static void grid_index(rec_t **recs, int n, double t0, int *ri, int *r2i, double *wi)
{
    int r = 0;
    for (int k = 0; k < GRID; k++) {
        double x = t0 + k / FS;
        while (r + 1 < n && recs[r + 1]->t <= x) r++;
        double w = 0; int r2 = r;
        if (x <= recs[0]->t) { r = r2 = 0; }
        else if (r + 1 < n) { r2 = r + 1; w = (x - recs[r]->t) / (recs[r2]->t - recs[r]->t); }
        ri[k] = r; r2i[k] = r2; wi[k] = w;
    }
}

static double amp_of(const rec_t *r, int c, int q) { return hypot(r->iq[c][q][0], r->iq[c][q][1]); }

/* Port 1-1 của features.invariant_features (3 chain); out[NF] theo thứ tự FNAMES. */
static void inv_features(rec_t **recs, int n, double t0, float *out)
{
    static double g[GRID][NONDC];
    int ri[GRID], r2i[GRID]; double wi[GRID], tc[GRID], tcc = 0;
    grid_index(recs, n, t0, ri, r2i, wi);
    for (int k = 0; k < GRID; k++) { tc[k] = k - (GRID - 1) / 2.0; tcc += tc[k] * tc[k]; }
    for (int c = 0; c < MAXCH; c++) {
        double inv[RING];
        for (int r = 0; r < n; r++) {
            double s = 0;
            for (int q = 0; q < NONDC; q++) s += amp_of(recs[r], c, q);
            inv[r] = 1.0 / (s / NONDC + EPS);
        }
        for (int k = 0; k < GRID; k++)
            for (int q = 0; q < NONDC; q++)
                g[k][q] = amp_of(recs[ri[k]], c, q) * inv[ri[k]] * (1 - wi[k]) + amp_of(recs[r2i[k]], c, q) * inv[r2i[k]] * wi[k];
        double sd[NONDC], sstd = 0, sstep = 0, pw = 0, pb = 0, pf = 0, pt = 0;
        for (int q = 0; q < NONDC; q++) {
            double m = 0, d[GRID], b = 0, v = 0, dm = 0;
            for (int k = 0; k < GRID; k++) m += g[k][q];
            m /= GRID;
            for (int k = 0; k < GRID; k++) { d[k] = g[k][q] - m; b += tc[k] * d[k]; }
            b /= tcc;
            for (int k = 0; k < GRID; k++) { d[k] -= tc[k] * b; dm += d[k]; }
            dm /= GRID;
            for (int k = 0; k < GRID; k++) v += (d[k] - dm) * (d[k] - dm);
            sd[q] = sqrt(v / GRID);
            sstd += sd[q];
            band_pow(d, &pw, &pb, &pf, &pt);
            for (int k = 1; k < GRID; k++) sstep += fabs(g[k][q] - g[k - 1][q]);
        }
        double p90 = percentile(sd, NONDC, 90), med = percentile(sd, NONDC, 50);
        pw /= NONDC; pb /= NONDC; pf /= NONDC; pt /= NONDC;
        out[PC * c] = (float)(sstd / NONDC);
        out[PC * c + 1] = (float)(sstep / (NONDC * (GRID - 1)));
        out[PC * c + 2] = (float)p90;
        out[PC * c + 3] = (float)(p90 / (med + EPS));
        out[PC * c + 4] = (float)pw;
        out[PC * c + 5] = (float)(pw / (pb + EPS));
        out[PC * c + 6] = (float)(pf / (pt + EPS));
    }
    /* CSI-ratio q = H0·conj(Hk): pha chung ngẫu nhiên (CFO/STO) giữa 2 anten cùng chip triệt tiêu */
    for (int k2 = 1; k2 < MAXCH; k2++) {
        double sstep = 0, sstd = 0, samp = 0, pw = 0, pb = 0, pf = 0, pt = 0;
        for (int q = 0; q < NONDC; q++) {
            double ure[GRID], uim[GRID], lr[GRID], mre = 0, mim = 0, lm = 0, lv = 0;
            for (int k = 0; k < GRID; k++) {
                double vr[2], vi[2], vl[2];
                const rec_t *rr[2] = {recs[ri[k]], recs[r2i[k]]};
                for (int j = 0; j < 2; j++) {
                    double ar = rr[j]->iq[0][q][0], ai = rr[j]->iq[0][q][1], br = rr[j]->iq[k2][q][0], bi = rr[j]->iq[k2][q][1];
                    double re = ar * br + ai * bi, im = ai * br - ar * bi, mag = hypot(re, im) + EPS;
                    vr[j] = re / mag; vi[j] = im / mag;
                    vl[j] = log(hypot(ar, ai) + 1) - log(hypot(br, bi) + 1);
                }
                double xr = vr[0] * (1 - wi[k]) + vr[1] * wi[k], xi = vi[0] * (1 - wi[k]) + vi[1] * wi[k];
                double mag = hypot(xr, xi) + EPS;
                ure[k] = xr / mag; uim[k] = xi / mag;
                lr[k] = vl[0] * (1 - wi[k]) + vl[1] * wi[k];
                mre += ure[k]; mim += uim[k]; lm += lr[k];
            }
            for (int k = 1; k < GRID; k++)
                sstep += fabs(atan2(uim[k] * ure[k - 1] - ure[k] * uim[k - 1], ure[k] * ure[k - 1] + uim[k] * uim[k - 1]));
            double R = hypot(mre / GRID, mim / GRID);
            R = R < 1e-9 ? 1e-9 : R > 1 ? 1 : R;
            sstd += sqrt(-2 * log(R));
            lm /= GRID;
            for (int k = 0; k < GRID; k++) lv += (lr[k] - lm) * (lr[k] - lm);
            samp += sqrt(lv / GRID);
            /* pha lệch quanh hướng trung bình: angle(u·conj(mean)), khử xu hướng, rồi dải tần */
            double ph[GRID], pm = 0, b = 0;
            for (int k = 0; k < GRID; k++) { ph[k] = atan2(uim[k] * mre - ure[k] * mim, ure[k] * mre + uim[k] * mim); pm += ph[k]; }
            pm /= GRID;
            for (int k = 0; k < GRID; k++) { ph[k] -= pm; b += tc[k] * ph[k]; }
            b /= tcc;
            for (int k = 0; k < GRID; k++) ph[k] -= tc[k] * b;
            band_pow(ph, &pw, &pb, &pf, &pt);
        }
        pw /= NONDC; pb /= NONDC;
        int o = PC * MAXCH + PP * (k2 - 1);
        out[o] = (float)(sstep / (NONDC * (GRID - 1)));
        out[o + 1] = (float)(sstd / NONDC);
        out[o + 2] = (float)(samp / NONDC);
        out[o + 3] = (float)pw;
        out[o + 4] = (float)(pw / (pb + EPS));
    }
}

/* đầu vào model theo feature_order.json: FMAP chỉ vào [raw NF | nb NF], nb = log((f+EPS)/(nền+EPS)) */
static int model_input(const float *f, float *x)
{
    float all[2 * NF];
    for (int i = 0; i < NF; i++) {
        all[i] = f[i];
        all[NF + i] = have_base ? (float)log((f[i] + EPS) / (base[i] + EPS)) : NAN;
    }
    for (int i = 0; i < CSI_RF_N_INPUT; i++) x[i] = all[FMAP[i]];
    return !NEEDS_BASELINE || have_base;
}

/* baseline.txt: mỗi dòng "<tên feature> <giá trị>" — đủ NF tên mới dùng */
static int load_baseline(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char name[64]; double v; int got = 0, seen[NF] = {0};
    while (fscanf(f, "%63s %lf", name, &v) == 2)
        for (int i = 0; i < NF; i++)
            if (!strcmp(name, FNAMES[i]) && !seen[i]) { base[i] = v; seen[i] = 1; got++; }
    fclose(f);
    have_base = got == NF;
    return have_base;
}

static int wave_index(void)
{
    for (int i = 0; i < CSI_RF_N_CLASSES; i++) if (!strcmp(csi_rf_labels[i], "wave")) return i;
    return -1;
}

/* Lấy các record trong [t0, t0+WIN_S] từ ring; trả số record, đủ phủ (coverage) mới dùng. */
static int window_records(double t0, rec_t **out)
{
    int n = 0;
    for (int i = 0; i < ring_n; i++) {
        rec_t *r = &ring[(ring_head - ring_n + i + RING) % RING];
        if (r->t >= t0 && r->t < t0 + WIN_S + 1e-9) out[n++] = r;
    }
    return n;
}

static int push_record(const csi_t *c, double t)
{
    if (ring_n) {
        rec_t *last = &ring[(ring_head - 1 + RING) % RING];
        if (t - last->t > MAX_GAP_US / 1e6 || t <= last->t) ring_n = 0; /* gap / TSF lùi: đoạn mới */
    }
    rec_t *r = &ring[ring_head];
    r->t = t;
    for (int ch = 0; ch < MAXCH; ch++) {
        const int16_t *p = c->iq + (size_t)ch * TONES * 2;
        for (int k = 0, o = 0; k < TONES; k++) {
            if (k % 53 == 26) continue; /* DC của mỗi khối 53 tone (features.NON_DC_MASK) */
            memcpy(r->iq[ch][o++], p + 2 * k, 4); /* relay không căn chỉnh: memcpy */
        }
    }
    ring_head = (ring_head + 1) % RING;
    if (ring_n < RING) ring_n++;
    return ring_n;
}

/* ---------------------------------------------------------------- replay (parity với Python) */
static int cmp_tsf(const void *a, const void *b)
{
    const csi_t *x = a, *y = b;
    if (x->tsf != y->tsf) return x->tsf < y->tsf ? -1 : 1;
    return x->iq < y->iq ? -1 : x->iq > y->iq; /* cùng TSF: giữ thứ tự trong file (như sort stable) */
}

static int replay(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    uint8_t *b = malloc(len);
    if (!b || fread(b, 1, len, f) != (size_t)len) { fprintf(stderr, "đọc lỗi %s\n", path); return 1; }
    fclose(f);
    init_tables();
    int wi = wave_index();
    double seg0 = -1, next = 0, t_prev = -1;
    uint64_t tsf0 = 0, last_tsf = 0; uint16_t last_ppdu = 0; int have_last = 0, nwin = 0;
    printf("window,start_s,n");
    for (int k = 0; k < NF; k++) printf(",%s", FNAMES[k]);
    printf(",p_wave\n");
    /* relay.bin của kit không theo thứ tự thời gian (ring relay quay vòng) → gom, sắp theo TSF, bỏ TSF lệch > 1 giờ
     * so với trung vị (record hỏng), như features.load_recording */
    size_t cap = 1024, nc = 0;
    csi_t *all = malloc(cap * sizeof *all);
    for (size_t i = 0; i < (size_t)len;) {
        csi_t c; int got;
        size_t used = parse_one(b + i, len - i, &c, &got);
        if (!used) break;
        i += used;
        if (!got) continue;
        if (nc == cap) all = realloc(all, (cap *= 2) * sizeof *all);
        all[nc++] = c;
    }
    qsort(all, nc, sizeof *all, cmp_tsf);
    uint64_t med = nc ? all[nc / 2].tsf : 0;
#ifdef WS_DUMP
    for (size_t j = 0; j < nc; j++) fprintf(stderr, "%llu %d %d %d\n", (unsigned long long)all[j].tsf, all[j].pkt_bw, all[j].ppdu, all[j].chains);
#endif
    for (size_t j = 0; j < nc; j++) {
        csi_t c = all[j];
        if ((c.tsf > med ? c.tsf - med : med - c.tsf) > 3600000000ULL) continue;
        if (c.pkt_bw != 2 || c.chains < MAXCH) continue;
        if (have_last && c.tsf == last_tsf && c.ppdu == last_ppdu) continue;
        have_last = 1; last_tsf = c.tsf; last_ppdu = c.ppdu;
        if (!tsf0) tsf0 = c.tsf;
        double t = (c.tsf - tsf0) / 1e6;
        if (t_prev < 0 || t - t_prev > MAX_GAP_US / 1e6 || t <= t_prev) { seg0 = t; next = 0; }
        push_record(&c, t);
        t_prev = t;
        /* như features.motion_windows: cửa sổ bắt đầu từ đầu đoạn, bước HOP_S, khi đã có record vượt cuối cửa sổ */
        while (seg0 + next + WIN_S <= t + 1e-9) {
            rec_t *w[RING];
            int n = window_records(seg0 + next, w);
            if (n >= MIN_COV * WIN_S / PERIOD_NOM) {
                float f[NF], x[CSI_RF_N_INPUT]; double p[CSI_RF_N_CLASSES];
                inv_features(w, n, seg0 + next, f);
                int ok = model_input(f, x);
                if (ok) csi_rf_predict(x, p);
                printf("%d,%.3f,%d", nwin++, seg0 + next, n);
                for (int k = 0; k < NF; k++) printf(",%.9g", f[k]);
                printf(",%.4f\n", ok && wi >= 0 ? p[wi] : NAN);
            }
            next += HOP_S;
        }
    }
    free(all);
    free(b);
    return 0;
}

/* ---------------------------------------------------------------- live */
static char ap_if[32] = "phy00.2-0", hw[32] = "ath12k_hw2", sta[24] = "auto", sta_ip[48] = "", action[512] = "";
static int period_ms = 50, keep_ms = 5, consecutive = 2, cooldown_s = 5;
static double on_th = 0.7, off_th = 0.4;
static char en_path[160], relay_path[160], req_path[256], base_path[160] = "/root/wifi_sensing/baseline.txt";
static int calib_s; /* > 0: chế độ hiệu chỉnh — gom feature calib_s giây rồi ghi nền, thoát */
static int calib_rc = 1;
#define MAXCAL 2048
#define BASE_PCT 20 /* như features.BASE_PCT: phân vị 20 % → chịu được người cử động một lúc khi hiệu chỉnh */
static float calib_buf[MAXCAL][NF];
static int calib_n;

/* nền = phân vị BASE_PCT từng feature trên các cửa sổ hiệu chỉnh; ghi tạm rồi rename (không để file dở) */
static int write_baseline(void)
{
    if (calib_n < 10) { fprintf(stderr, "wifi_sensing: chỉ %d cửa sổ khi hiệu chỉnh (cần ≥ 10) — station/sóng yếu?\n", calib_n); return 1; }
    char tmp[176];
    snprintf(tmp, sizeof tmp, "%s.tmp", base_path);
    FILE *f = fopen(tmp, "w");
    if (!f) { perror(tmp); return 1; }
    static double col[MAXCAL];
    for (int i = 0; i < NF; i++) {
        for (int k = 0; k < calib_n; k++) col[k] = calib_buf[k][i];
        fprintf(f, "%s %.9g\n", FNAMES[i], percentile(col, calib_n, BASE_PCT));
    }
    fclose(f);
    if (rename(tmp, base_path)) { perror(base_path); return 1; }
    fprintf(stderr, "wifi_sensing: đã ghi nền %s từ %d cửa sổ\n", base_path, calib_n);
    return 0;
}

static int write_str(const char *path, const char *s)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int ok = write(fd, s, strlen(s)) == (ssize_t)strlen(s);
    close(fd);
    return ok ? 0 : -1;
}

static int read_str(const char *path, char *buf, size_t n)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t r = read(fd, buf, n - 1);
    close(fd);
    buf[r > 0 ? r : 0] = 0;
    return r > 0 ? 0 : -1;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* station: MAC cấu hình, hoặc "auto" = station đầu tiên trên ap_if */
static int find_station(char *mac)
{
    char dir[160];
    snprintf(dir, sizeof dir, "/sys/kernel/debug/ieee80211/phy00/netdev:%s/stations", ap_if);
    if (strcmp(sta, "auto")) {
        snprintf(req_path, sizeof req_path, "%s/%s/link-0/cfr_capture", dir, sta);
        strcpy(mac, sta);
        return access(req_path, W_OK) == 0;
    }
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)))
        if (strlen(e->d_name) == 17) {
            snprintf(req_path, sizeof req_path, "%s/%s/link-0/cfr_capture", dir, e->d_name);
            if (access(req_path, W_OK) == 0) { strcpy(mac, e->d_name); found = 1; break; }
        }
    closedir(d);
    return found;
}

/* IP của station cho keep-awake: cấu hình, hoặc tra /tmp/dhcp.leases rồi /proc/net/arp */
static int find_ip(const char *mac, char *ip)
{
    if (sta_ip[0]) { strcpy(ip, sta_ip); return 1; }
    char line[256], a[64], b[64], c[64];
    FILE *f = fopen("/tmp/dhcp.leases", "r");
    if (f) {
        while (fgets(line, sizeof line, f))
            if (sscanf(line, "%63s %63s %63s", a, b, c) == 3 && !strcasecmp(b, mac)) { strcpy(ip, c); fclose(f); return 1; }
        fclose(f);
    }
    if ((f = fopen("/proc/net/arp", "r"))) {
        while (fgets(line, sizeof line, f))
            if (sscanf(line, "%63s %*s %*s %63s", a, b) == 2 && !strcasecmp(b, mac)) { strcpy(ip, a); fclose(f); return 1; }
        fclose(f);
    }
    return 0;
}

/* bộ đếm chẩn đoán của phiên hiện tại (ghi vào status): CSI parse được / đạt 80 MHz / cửa sổ đủ phủ / thiếu phủ */
static long n_csi, n_bw80, n_win, n_win_low;

static void write_status(const char *state, const char *mac, double rate, double p, long detections)
{
    FILE *f = fopen("/tmp/wifi_sensing.status.tmp", "w");
    if (!f) return;
    fprintf(f, "{\"state\":\"%s\",\"station\":\"%s\",\"ap_if\":\"%s\",\"rate\":%.1f,\"p_wave\":%.3f,"
               "\"detections\":%ld,\"csi\":%ld,\"csi_bw80\":%ld,\"windows\":%ld,\"windows_low_coverage\":%ld,"
               "\"time\":%ld}\n", state, mac, ap_if, rate, p, detections, n_csi, n_bw80, n_win, n_win_low,
            (long)time(NULL));
    fclose(f);
    rename("/tmp/wifi_sensing.status.tmp", "/tmp/wifi_sensing.status");
}

static void run_action(double p, const char *mac)
{
    char ps[32];
    snprintf(ps, sizeof ps, "%.3f", p);
    fprintf(stderr, "wifi_sensing: WAVE p=%s station=%s\n", ps, mac);
    pid_t pid = fork();
    if (pid == 0) {
        char ts[32];
        snprintf(ts, sizeof ts, "%ld", (long)time(NULL));
        setenv("WS_P", ps, 1);
        setenv("WS_STATION", mac, 1);
        setenv("WS_TS", ts, 1);
        execl("/bin/sh", "sh", "-c", action, (char *)NULL);
        _exit(127);
    }
}

static void on_signal(int s) { (void)s; stop_flag = 1; }

static void stop_capture(void)
{
    if (req_path[0]) write_str(req_path, "0");
    write_str(en_path, "0");
}

/* Một phiên thu với một station; trả khi station mất / lỗi / dừng. */
static void session(const char *mac, long *detections)
{
    char ip[64] = "", buf[64];
    if (read_str(en_path, buf, sizeof buf) == 0 && atoi(buf) != 0) {
        fprintf(stderr, "wifi_sensing: enable_cfr đã = 1 (người khác đang thu), chờ\n");
        sleep(10);
        return;
    }
    if (write_str(en_path, "1")) { fprintf(stderr, "wifi_sensing: không bật được %s\n", en_path); sleep(10); return; }
    int rfd = open(relay_path, O_RDONLY | O_NONBLOCK);
    if (rfd < 0) { perror(relay_path); stop_capture(); sleep(10); return; }
    snprintf(buf, sizeof buf, "1 2 %d 0", period_ms);
    if (write_str(req_path, buf)) { fprintf(stderr, "wifi_sensing: request CFR thất bại (%s)\n", mac); close(rfd); stop_capture(); return; }
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in dst = {.sin_family = AF_INET, .sin_port = htons(9)};
    int have_ip = find_ip(mac, ip) && inet_pton(AF_INET, ip, &dst.sin_addr) == 1;
    fprintf(stderr, "wifi_sensing: thu từ %s (keep-awake %s)\n", mac, have_ip ? ip : "tắt: không biết IP");
    static uint8_t rbuf[1 << 17];
    static char pad[1200];
    size_t rlen = 0;
    int wi = wave_index(), above = 0, active = 0;
    double next_ka = 0, last_check = now_s(), last_fire = -1e9, next_win = -1, t_prev = -1, p = 0;
    uint64_t tsf0 = 0, last_tsf = 0; uint16_t last_ppdu = 0;
    long nrec = 0; double rate_t0 = now_s(), next_status = 0;
    n_csi = n_bw80 = n_win = n_win_low = 0;
    ring_n = 0;
    double calib_end = calib_s > 0 ? now_s() + calib_s : 0;
    write_status(calib_s > 0 ? "calibrating" : "capturing", mac, 0, 0, *detections);
    while (!stop_flag) {
        double now = now_s();
        if (have_ip && keep_ms > 0 && now >= next_ka) {
            sendto(sock, pad, sizeof pad, MSG_DONTWAIT, (struct sockaddr *)&dst, sizeof dst);
            next_ka = now + keep_ms / 1000.0;
        }
        if (now - last_check > 2) { /* station còn không? */
            last_check = now;
            if (access(req_path, W_OK)) { fprintf(stderr, "wifi_sensing: station %s rời mạng\n", mac); break; }
        }
        struct pollfd pf = {.fd = rfd, .events = POLLIN};
        poll(&pf, 1, keep_ms > 0 ? keep_ms : 100);
        ssize_t r = read(rfd, rbuf + rlen, sizeof rbuf - rlen);
        if (r > 0) rlen += r;
        size_t off = 0;
        for (;;) {
            csi_t c; int got;
            size_t used = parse_one(rbuf + off, rlen - off, &c, &got);
            if (!used) break;
            off += used;
            if (!got) continue;
            n_csi++;
            if (c.pkt_bw != 2 || c.chains < MAXCH) continue;
            n_bw80++;
            if (c.tsf == last_tsf && c.ppdu == last_ppdu) continue;
            last_tsf = c.tsf; last_ppdu = c.ppdu;
            if (!tsf0) tsf0 = c.tsf;
            double t = (c.tsf - tsf0) / 1e6;
            push_record(&c, t);
            nrec++;
            if (t_prev < 0 || t - t_prev > MAX_GAP_US / 1e6 || t <= t_prev) next_win = t + WIN_S; /* đoạn mới */
            t_prev = t;
            if (t < next_win) continue;
            next_win = t + HOP_S;
            rec_t *w[RING];
            double t0 = t - WIN_S;
            int n = window_records(t0, w);
            if (n < MIN_COV * WIN_S / PERIOD_NOM) { n_win_low++; continue; }
            n_win++;
            float f[NF], x[CSI_RF_N_INPUT]; double pr[CSI_RF_N_CLASSES];
            inv_features(w, n, t0, f);
            if (calib_s > 0) { if (calib_n < MAXCAL) memcpy(calib_buf[calib_n++], f, sizeof f); continue; }
            if (!model_input(f, x)) continue; /* model cần nền mà chưa hiệu chỉnh */
            csi_rf_predict(x, pr);
            p = wi >= 0 ? pr[wi] : 0;
            /* hysteresis: bật khi p ≥ on_th ở `consecutive` cửa sổ liền nhau, tắt khi p < off_th */
            above = p >= on_th ? above + 1 : 0;
            if (!active && above >= consecutive) {
                active = 1;
                if (now - last_fire >= cooldown_s) { last_fire = now; (*detections)++; if (action[0]) run_action(p, mac); }
            } else if (active && p < off_th) active = 0;
        }
        if (calib_s > 0 && now >= calib_end) { calib_rc = write_baseline(); stop_flag = 1; break; }
        if (now >= next_status) { /* mỗi 2 s, kể cả khi không có cửa sổ nào đủ phủ */
            write_status(calib_s > 0 ? "calibrating" : active ? "wave" : "capturing", mac, nrec / (now - rate_t0 + 1e-9), p, *detections);
            next_status = now + 2;
        }
        memmove(rbuf, rbuf + off, rlen - off);
        rlen -= off;
        if (rlen == sizeof rbuf) rlen = 0; /* rác không parse được: bỏ */
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
    }
    close(sock);
    close(rfd);
    stop_capture();
}

int main(int argc, char **argv)
{
    const char *replay_path = NULL;
    for (int i = 1; i < argc; i++) {
        const char *v = i + 1 < argc ? argv[i + 1] : "";
        if (!strcmp(argv[i], "--replay")) replay_path = v, i++;
        else if (!strcmp(argv[i], "-b")) snprintf(base_path, sizeof base_path, "%s", v), i++;
        else if (!strcmp(argv[i], "--calibrate")) calib_s = atoi(v), i++;
        else if (!strcmp(argv[i], "-i")) snprintf(ap_if, sizeof ap_if, "%s", v), i++;
        else if (!strcmp(argv[i], "-H")) snprintf(hw, sizeof hw, "%s", v), i++;
        else if (!strcmp(argv[i], "-s")) snprintf(sta, sizeof sta, "%s", v[0] ? v : "auto"), i++;
        else if (!strcmp(argv[i], "-a")) snprintf(sta_ip, sizeof sta_ip, "%s", v), i++;
        else if (!strcmp(argv[i], "-x")) snprintf(action, sizeof action, "%s", v), i++;
        else if (!strcmp(argv[i], "-p")) period_ms = atoi(v), i++;
        else if (!strcmp(argv[i], "-k")) keep_ms = atoi(v), i++;
        else if (!strcmp(argv[i], "-n")) consecutive = atoi(v), i++;
        else if (!strcmp(argv[i], "-C")) cooldown_s = atoi(v), i++;
        else if (!strcmp(argv[i], "-t")) on_th = atof(v), i++;
        else if (!strcmp(argv[i], "-T")) off_th = atof(v), i++;
        else { fprintf(stderr, "tham số lạ: %s (xem đầu file wifi_sensing.c)\n", argv[i]); return 2; }
    }
    if (replay_path) { load_baseline(base_path); return replay(replay_path); }
    if (period_ms < 20 || period_ms > 1000 || off_th > on_th || consecutive < 1 || calib_s < 0) {
        fprintf(stderr, "cấu hình sai: period 20-1000 ms, off_threshold ≤ on_threshold, consecutive ≥ 1\n");
        return 2;
    }
    snprintf(en_path, sizeof en_path, "/sys/kernel/debug/ieee80211/phy00/%s/enable_cfr", hw);
    snprintf(relay_path, sizeof relay_path, "/sys/kernel/debug/ieee80211/phy00/%s/cfr_capture0", hw);
    char dbr[8];
    if (read_str("/sys/module/ath12k/parameters/cfr_dbr_only", dbr, sizeof dbr) || dbr[0] != 'Y') {
        fprintf(stderr, "wifi_sensing: cfr_dbr_only khác Y — driver CFR patched chưa chạy (/etc/init.d/cfr9650_driver)\n");
        return 1;
    }
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);
    signal(SIGPIPE, SIG_IGN);
    init_tables();
    long detections = 0;
    char mac[24];
    fprintf(stderr, "wifi_sensing: %s/%s, chu kỳ %d ms, ngưỡng %.2f/%.2f x%d, model %d lớp, %s\n",
            ap_if, hw, period_ms, on_th, off_th, consecutive, CSI_RF_N_CLASSES,
            calib_s > 0 ? "HIỆU CHỈNH" : NEEDS_BASELINE ? "cần nền" : "không cần nền");
    /* model chuẩn hoá theo nền mà chưa có baseline.txt: chờ (không thoát, tránh procd respawn liên tục) */
    while (!calib_s && NEEDS_BASELINE && !load_baseline(base_path) && !stop_flag) {
        fprintf(stderr, "wifi_sensing: chưa có nền %s — chạy '/etc/init.d/wifi_sensing calibrate' khi phòng yên\n", base_path);
        write_status("need_calibration", "", 0, 0, 0);
        for (int k = 0; k < 30 && !stop_flag; k++) sleep(1);
    }
    while (!stop_flag) {
        if (!find_station(mac)) { write_status("no_station", "", 0, 0, detections); sleep(5); continue; }
        session(mac, &detections);
        if (!stop_flag) sleep(1);
    }
    stop_capture();
    write_status("stopped", "", 0, 0, detections);
    return calib_s > 0 ? calib_rc : 0;
}
