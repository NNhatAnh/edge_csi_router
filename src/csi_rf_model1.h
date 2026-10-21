#ifndef CSI_RF_MODEL1_H
#define CSI_RF_MODEL1_H

#ifdef __cplusplus
extern "C" {
#endif

#define CSI_RF1_N_INPUT 9
#define CSI_RF1_N_CLASSES 2
#define CSI_RF1_DEFAULT_THRESHOLD 0.50

/* Features in order (motion_core, 3 chains):
 * 0: motion_std_c0
 * 1: motion_step_c0
 * 2: motion_std_c1
 * 3: motion_step_c1
 * 4: motion_std_c2
 * 5: motion_step_c2
 * 6: motion_c0_log_power
 * 7: motion_c1_log_power
 * 8: motion_c2_log_power
 */

int csi_rf1_predict(const float *input, double *proba);
int csi_rf1_predict_thresh(const float *input, double *proba, double threshold);

#ifdef __cplusplus
}
#endif

#endif // CSI_RF_MODEL1_H
