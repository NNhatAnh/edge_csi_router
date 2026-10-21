#ifndef CSI_RF_MODEL1_H
#define CSI_RF_MODEL1_H

#ifdef __cplusplus
extern "C" {
#endif

#define CSI_RF1_N_INPUT 5
#define CSI_RF1_N_CLASSES 2
#define CSI_RF1_DEFAULT_THRESHOLD 0.35f

/*
 * Features in order (sparse_wave, model_package/model/wave_rf.joblib):
 * 0: relative_delta_rate_c0   (Chain 0 normalized amplitude delta rate)
 * 1: relative_delta_rate_c1   (Chain 1 normalized amplitude delta rate)
 * 2: relative_delta_rate_c2   (Chain 2 normalized amplitude delta rate)
 * 3: shape_step_rate_c0       (Chain 0 subcarrier shape step rate)
 * 4: shape_step_rate_c2       (Chain 2 subcarrier shape step rate)
 */

int csi_rf1_predict(const float *input, double *proba);

#ifdef __cplusplus
}
#endif

#endif // CSI_RF_MODEL1_H
