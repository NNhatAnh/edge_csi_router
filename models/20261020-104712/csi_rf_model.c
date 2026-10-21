#include <math.h>
#define CSI_RF_N_INPUT 84
#define CSI_RF_N_CLASSES 2
const char *csi_rf_labels[CSI_RF_N_CLASSES] = {"no_wave", "wave"};
static const int feature_mapping[84] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83};
static const double imputation[84] = {1205.1282398953322, 6.7915141189691353, 0.13497670760666983, 7.2053506645352963, 0.33173544150994921, 7.3306923205265457, 0.4145945832577107, 6.6766445669615369, 0.12010683413658427, 0.98240194880283571, 0.11126711718007154, 0.0025929014272494731, 0.23322261873205433, 0.45398106816679923, 0.14000481801985973, 0.0095987678247692557, 0.10932051045173848, 0.5283911194070372, 0.067466548647138691, 0.48430020357474923, 0.29265621452129836, 0.2478018797026551, 0.8381606992184476, 0.14688433902678305, -4.3764009268509296, 1311.688410601947, 7.2191276161095139, 0.27536555577968103, 7.3624107036764501, 0.36215075718589329, 7.1491266082489071, 0.23127050859593593, 6.8437096166756657, 0.12909604754274728, 0.99446818886897015, 0.046777334226755014, 0.016365673453288321, 0.10969059850570412, 0.14440403118917866, 0.095550395860015777, 0.002928935110028015, 0.072454351946343462, 0.36443733121277533, 0.043275403488335064, 0.44143244447806224, 0.33579912805854989, 0.25453601114194691, 0.86000696648501407, 0.17630671492628669, -6.3608308651882366, 1242.6296170343967, 7.1858354428725599, 0.27274638502509474, 7.3928874052384979, 0.4110871411038029, 7.035873420644088, 0.20736662504305572, 6.6532368042928169, 0.095120711785558962, 0.98445418879212232, 0.093614026951126833, 0.024537638008145013, 0.16816483079771527, 0.39940815440027932, 0.10761951260061053, 0.0075404135768014479, 0.056523867754687748, 0.56599189182589904, 0.13205119272298069, 0.48953798301890061, 0.25380985305757725, 0.22812271652530197, 0.8437557773187242, 0.139594612130404, -4.9268815109635034, -0.087000186092423368, -0.43435785225853851, 0.15349409961444888, -0.055214886962609032, 0.47625161286303236, 0.09404751556366811, 0.051307448213985175, -0.37902876728321261, 0.11135943256170168};
static const int t0_children_left[1] = {-1};
static const int t0_children_right[1] = {-1};
static const int t0_feature[1] = {-2};
static const double t0_threshold[1] = {-2};
static const double t0_probabilities[2] = {0.23529411764705882, 0.76470588235294112};
static const int t1_children_left[3] = {1, -1, -1};
static const int t1_children_right[3] = {2, -1, -1};
static const int t1_feature[3] = {76, -2, -2};
static const double t1_threshold[3] = {-0.32521891593933105, -2, -2};
static const double t1_probabilities[6] = {0.52941176470588236, 0.47058823529411764, 1, 0, 0.20000000000000001, 0.80000000000000004};
static const int t2_children_left[3] = {1, -1, -1};
static const int t2_children_right[3] = {2, -1, -1};
static const int t2_feature[3] = {52, -2, -2};
static const double t2_threshold[3] = {0.27960775792598724, -2, -2};
static const double t2_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t3_children_left[3] = {1, -1, -1};
static const int t3_children_right[3] = {2, -1, -1};
static const int t3_feature[3] = {49, -2, -2};
static const double t3_threshold[3] = {-6.1700825691223145, -2, -2};
static const double t3_probabilities[6] = {0.41176470588235292, 0.58823529411764708, 1, 0, 0.090909090909090912, 0.90909090909090906};
static const int t4_children_left[3] = {1, -1, -1};
static const int t4_children_right[3] = {2, -1, -1};
static const int t4_feature[3] = {82, -2, -2};
static const double t4_threshold[3] = {-0.29926641285419464, -2, -2};
static const double t4_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t5_children_left[3] = {1, -1, -1};
static const int t5_children_right[3] = {2, -1, -1};
static const int t5_feature[3] = {10, -2, -2};
static const double t5_threshold[3] = {0.10398247092962265, -2, -2};
static const double t5_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 0.22222222222222221, 0.77777777777777779, 1, 0};
static const int t6_children_left[3] = {1, -1, -1};
static const int t6_children_right[3] = {2, -1, -1};
static const int t6_feature[3] = {40, -2, -2};
static const double t6_threshold[3] = {0.0045405768323689699, -2, -2};
static const double t6_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t7_children_left[3] = {1, -1, -1};
static const int t7_children_right[3] = {2, -1, -1};
static const int t7_feature[3] = {22, -2, -2};
static const double t7_threshold[3] = {0.82641848921775818, -2, -2};
static const double t7_probabilities[6] = {0.70588235294117652, 0.29411764705882354, 1, 0, 0.375, 0.625};
static const int t8_children_left[1] = {-1};
static const int t8_children_right[1] = {-1};
static const int t8_feature[1] = {-2};
static const double t8_threshold[1] = {-2};
static const double t8_probabilities[2] = {0.52941176470588236, 0.47058823529411764};
static const int t9_children_left[1] = {-1};
static const int t9_children_right[1] = {-1};
static const int t9_feature[1] = {-2};
static const double t9_threshold[1] = {-2};
static const double t9_probabilities[2] = {0.35294117647058826, 0.6470588235294118};
static const int t10_children_left[1] = {-1};
static const int t10_children_right[1] = {-1};
static const int t10_feature[1] = {-2};
static const double t10_threshold[1] = {-2};
static const double t10_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t11_children_left[3] = {1, -1, -1};
static const int t11_children_right[3] = {2, -1, -1};
static const int t11_feature[3] = {40, -2, -2};
static const double t11_threshold[3] = {0.0043093726271763444, -2, -2};
static const double t11_probabilities[6] = {0.41176470588235292, 0.58823529411764708, 1, 0, 0.090909090909090912, 0.90909090909090906};
static const int t12_children_left[1] = {-1};
static const int t12_children_right[1] = {-1};
static const int t12_feature[1] = {-2};
static const double t12_threshold[1] = {-2};
static const double t12_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t13_children_left[1] = {-1};
static const int t13_children_right[1] = {-1};
static const int t13_feature[1] = {-2};
static const double t13_threshold[1] = {-2};
static const double t13_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t14_children_left[3] = {1, -1, -1};
static const int t14_children_right[3] = {2, -1, -1};
static const int t14_feature[3] = {37, -2, -2};
static const double t14_threshold[3] = {0.12501449137926102, -2, -2};
static const double t14_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.18181818181818182, 0.81818181818181823};
static const int t15_children_left[1] = {-1};
static const int t15_children_right[1] = {-1};
static const int t15_feature[1] = {-2};
static const double t15_threshold[1] = {-2};
static const double t15_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t16_children_left[1] = {-1};
static const int t16_children_right[1] = {-1};
static const int t16_feature[1] = {-2};
static const double t16_threshold[1] = {-2};
static const double t16_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t17_children_left[1] = {-1};
static const int t17_children_right[1] = {-1};
static const int t17_feature[1] = {-2};
static const double t17_threshold[1] = {-2};
static const double t17_probabilities[2] = {0.6470588235294118, 0.35294117647058826};
static const int t18_children_left[1] = {-1};
static const int t18_children_right[1] = {-1};
static const int t18_feature[1] = {-2};
static const double t18_threshold[1] = {-2};
static const double t18_probabilities[2] = {0.58823529411764708, 0.41176470588235292};
static const int t19_children_left[1] = {-1};
static const int t19_children_right[1] = {-1};
static const int t19_feature[1] = {-2};
static const double t19_threshold[1] = {-2};
static const double t19_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t20_children_left[3] = {1, -1, -1};
static const int t20_children_right[3] = {2, -1, -1};
static const int t20_feature[3] = {46, -2, -2};
static const double t20_threshold[3] = {0.27036619186401367, -2, -2};
static const double t20_probabilities[6] = {0.70588235294117652, 0.29411764705882354, 0.2857142857142857, 0.7142857142857143, 1, 0};
static const int t21_children_left[1] = {-1};
static const int t21_children_right[1] = {-1};
static const int t21_feature[1] = {-2};
static const double t21_threshold[1] = {-2};
static const double t21_probabilities[2] = {0.29411764705882354, 0.70588235294117652};
static const int t22_children_left[1] = {-1};
static const int t22_children_right[1] = {-1};
static const int t22_feature[1] = {-2};
static const double t22_threshold[1] = {-2};
static const double t22_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t23_children_left[3] = {1, -1, -1};
static const int t23_children_right[3] = {2, -1, -1};
static const int t23_feature[3] = {68, -2, -2};
static const double t23_threshold[3] = {0.056391919031739235, -2, -2};
static const double t23_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 0.125, 0.875, 1, 0};
static const int t24_children_left[3] = {1, -1, -1};
static const int t24_children_right[3] = {2, -1, -1};
static const int t24_feature[3] = {76, -2, -2};
static const double t24_threshold[3] = {-0.32088831067085266, -2, -2};
static const double t24_probabilities[6] = {0.6470588235294118, 0.35294117647058826, 1, 0, 0.25, 0.75};
static const int t25_children_left[3] = {1, -1, -1};
static const int t25_children_right[3] = {2, -1, -1};
static const int t25_feature[3] = {77, -2, -2};
static const double t25_threshold[3] = {0.15077248215675354, -2, -2};
static const double t25_probabilities[6] = {0.6470588235294118, 0.35294117647058826, 0.33333333333333331, 0.66666666666666663, 1, 0};
static const int t26_children_left[3] = {1, -1, -1};
static const int t26_children_right[3] = {2, -1, -1};
static const int t26_feature[3] = {76, -2, -2};
static const double t26_threshold[3] = {-0.28876599669456482, -2, -2};
static const double t26_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t27_children_left[1] = {-1};
static const int t27_children_right[1] = {-1};
static const int t27_feature[1] = {-2};
static const double t27_threshold[1] = {-2};
static const double t27_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t28_children_left[3] = {1, -1, -1};
static const int t28_children_right[3] = {2, -1, -1};
static const int t28_feature[3] = {52, -2, -2};
static const double t28_threshold[3] = {0.27960775792598724, -2, -2};
static const double t28_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.10000000000000001, 0.90000000000000002};
static const int t29_children_left[3] = {1, -1, -1};
static const int t29_children_right[3] = {2, -1, -1};
static const int t29_feature[3] = {51, -2, -2};
static const double t29_threshold[3] = {7.1793551445007324, -2, -2};
static const double t29_probabilities[6] = {0.41176470588235292, 0.58823529411764708, 1, 0, 0.090909090909090912, 0.90909090909090906};
static const int t30_children_left[1] = {-1};
static const int t30_children_right[1] = {-1};
static const int t30_feature[1] = {-2};
static const double t30_threshold[1] = {-2};
static const double t30_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t31_children_left[3] = {1, -1, -1};
static const int t31_children_right[3] = {2, -1, -1};
static const int t31_feature[3] = {40, -2, -2};
static const double t31_threshold[3] = {0.0028648829320445657, -2, -2};
static const double t31_probabilities[6] = {0.76470588235294112, 0.23529411764705882, 1, 0, 0.33333333333333331, 0.66666666666666663};
static const int t32_children_left[3] = {1, -1, -1};
static const int t32_children_right[3] = {2, -1, -1};
static const int t32_feature[3] = {13, -2, -2};
static const double t32_threshold[3] = {0.44893167912960052, -2, -2};
static const double t32_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.22222222222222221, 0.77777777777777779};
static const int t33_children_left[1] = {-1};
static const int t33_children_right[1] = {-1};
static const int t33_feature[1] = {-2};
static const double t33_threshold[1] = {-2};
static const double t33_probabilities[2] = {0.52941176470588236, 0.47058823529411764};
static const int t34_children_left[3] = {1, -1, -1};
static const int t34_children_right[3] = {2, -1, -1};
static const int t34_feature[3] = {82, -2, -2};
static const double t34_threshold[3] = {-0.31064815819263458, -2, -2};
static const double t34_probabilities[6] = {0.52941176470588236, 0.47058823529411764, 1, 0, 0.27272727272727271, 0.72727272727272729};
static const int t35_children_left[3] = {1, -1, -1};
static const int t35_children_right[3] = {2, -1, -1};
static const int t35_feature[3] = {40, -2, -2};
static const double t35_threshold[3] = {0.0029748950619250536, -2, -2};
static const double t35_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.10000000000000001, 0.90000000000000002};
static const int t36_children_left[1] = {-1};
static const int t36_children_right[1] = {-1};
static const int t36_feature[1] = {-2};
static const double t36_threshold[1] = {-2};
static const double t36_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t37_children_left[1] = {-1};
static const int t37_children_right[1] = {-1};
static const int t37_feature[1] = {-2};
static const double t37_threshold[1] = {-2};
static const double t37_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t38_children_left[1] = {-1};
static const int t38_children_right[1] = {-1};
static const int t38_feature[1] = {-2};
static const double t38_threshold[1] = {-2};
static const double t38_probabilities[2] = {0.35294117647058826, 0.6470588235294118};
static const int t39_children_left[3] = {1, -1, -1};
static const int t39_children_right[3] = {2, -1, -1};
static const int t39_feature[3] = {76, -2, -2};
static const double t39_threshold[3] = {-0.28876599669456482, -2, -2};
static const double t39_probabilities[6] = {0.41176470588235292, 0.58823529411764708, 1, 0, 0.090909090909090912, 0.90909090909090906};
static const int t40_children_left[3] = {1, -1, -1};
static const int t40_children_right[3] = {2, -1, -1};
static const int t40_feature[3] = {15, -2, -2};
static const double t40_threshold[3] = {0.011717014946043491, -2, -2};
static const double t40_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.18181818181818182, 0.81818181818181823};
static const int t41_children_left[1] = {-1};
static const int t41_children_right[1] = {-1};
static const int t41_feature[1] = {-2};
static const double t41_threshold[1] = {-2};
static const double t41_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t42_children_left[1] = {-1};
static const int t42_children_right[1] = {-1};
static const int t42_feature[1] = {-2};
static const double t42_threshold[1] = {-2};
static const double t42_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t43_children_left[3] = {1, -1, -1};
static const int t43_children_right[3] = {2, -1, -1};
static const int t43_feature[3] = {49, -2, -2};
static const double t43_threshold[3] = {-6.1095640659332275, -2, -2};
static const double t43_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.22222222222222221, 0.77777777777777779};
static const int t44_children_left[3] = {1, -1, -1};
static const int t44_children_right[3] = {2, -1, -1};
static const int t44_feature[3] = {14, -2, -2};
static const double t44_threshold[3] = {0.17625753581523895, -2, -2};
static const double t44_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.10000000000000001, 0.90000000000000002};
static const int t45_children_left[1] = {-1};
static const int t45_children_right[1] = {-1};
static const int t45_feature[1] = {-2};
static const double t45_threshold[1] = {-2};
static const double t45_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t46_children_left[3] = {1, -1, -1};
static const int t46_children_right[3] = {2, -1, -1};
static const int t46_feature[3] = {49, -2, -2};
static const double t46_threshold[3] = {-5.9138379096984863, -2, -2};
static const double t46_probabilities[6] = {0.6470588235294118, 0.35294117647058826, 1, 0, 0.14285714285714285, 0.8571428571428571};
static const int t47_children_left[1] = {-1};
static const int t47_children_right[1] = {-1};
static const int t47_feature[1] = {-2};
static const double t47_threshold[1] = {-2};
static const double t47_probabilities[2] = {0.35294117647058826, 0.6470588235294118};
static const int t48_children_left[1] = {-1};
static const int t48_children_right[1] = {-1};
static const int t48_feature[1] = {-2};
static const double t48_threshold[1] = {-2};
static const double t48_probabilities[2] = {0.35294117647058826, 0.6470588235294118};
static const int t49_children_left[3] = {1, -1, -1};
static const int t49_children_right[3] = {2, -1, -1};
static const int t49_feature[3] = {64, -2, -2};
static const double t49_threshold[3] = {0.12147065997123718, -2, -2};
static const double t49_probabilities[6] = {0.41176470588235292, 0.58823529411764708, 1, 0, 0.090909090909090912, 0.90909090909090906};
static const int t50_children_left[3] = {1, -1, -1};
static const int t50_children_right[3] = {2, -1, -1};
static const int t50_feature[3] = {4, -2, -2};
static const double t50_threshold[3] = {0.3324398547410965, -2, -2};
static const double t50_probabilities[6] = {0.52941176470588236, 0.47058823529411764, 0.1111111111111111, 0.88888888888888884, 1, 0};
static const int t51_children_left[3] = {1, -1, -1};
static const int t51_children_right[3] = {2, -1, -1};
static const int t51_feature[3] = {68, -2, -2};
static const double t51_threshold[3] = {0.056391919031739235, -2, -2};
static const double t51_probabilities[6] = {0.41176470588235292, 0.58823529411764708, 0.090909090909090912, 0.90909090909090906, 1, 0};
static const int t52_children_left[3] = {1, -1, -1};
static const int t52_children_right[3] = {2, -1, -1};
static const int t52_feature[3] = {64, -2, -2};
static const double t52_threshold[3] = {0.10204936936497688, -2, -2};
static const double t52_probabilities[6] = {0.52941176470588236, 0.47058823529411764, 1, 0, 0.20000000000000001, 0.80000000000000004};
static const int t53_children_left[1] = {-1};
static const int t53_children_right[1] = {-1};
static const int t53_feature[1] = {-2};
static const double t53_threshold[1] = {-2};
static const double t53_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t54_children_left[1] = {-1};
static const int t54_children_right[1] = {-1};
static const int t54_feature[1] = {-2};
static const double t54_threshold[1] = {-2};
static const double t54_probabilities[2] = {0.52941176470588236, 0.47058823529411764};
static const int t55_children_left[3] = {1, -1, -1};
static const int t55_children_right[3] = {2, -1, -1};
static const int t55_feature[3] = {20, -2, -2};
static const double t55_threshold[3] = {0.28523561358451843, -2, -2};
static const double t55_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.10000000000000001, 0.90000000000000002};
static const int t56_children_left[1] = {-1};
static const int t56_children_right[1] = {-1};
static const int t56_feature[1] = {-2};
static const double t56_threshold[1] = {-2};
static const double t56_probabilities[2] = {0.29411764705882354, 0.70588235294117652};
static const int t57_children_left[3] = {1, -1, -1};
static const int t57_children_right[3] = {2, -1, -1};
static const int t57_feature[3] = {10, -2, -2};
static const double t57_threshold[3] = {0.098548926413059235, -2, -2};
static const double t57_probabilities[6] = {0.76470588235294112, 0.23529411764705882, 0.20000000000000001, 0.80000000000000004, 1, 0};
static const int t58_children_left[1] = {-1};
static const int t58_children_right[1] = {-1};
static const int t58_feature[1] = {-2};
static const double t58_threshold[1] = {-2};
static const double t58_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t59_children_left[3] = {1, -1, -1};
static const int t59_children_right[3] = {2, -1, -1};
static const int t59_feature[3] = {37, -2, -2};
static const double t59_threshold[3] = {0.12065129727125168, -2, -2};
static const double t59_probabilities[6] = {0.70588235294117652, 0.29411764705882354, 1, 0, 0.2857142857142857, 0.7142857142857143};
static const int t60_children_left[1] = {-1};
static const int t60_children_right[1] = {-1};
static const int t60_feature[1] = {-2};
static const double t60_threshold[1] = {-2};
static const double t60_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t61_children_left[1] = {-1};
static const int t61_children_right[1] = {-1};
static const int t61_feature[1] = {-2};
static const double t61_threshold[1] = {-2};
static const double t61_probabilities[2] = {0.35294117647058826, 0.6470588235294118};
static const int t62_children_left[3] = {1, -1, -1};
static const int t62_children_right[3] = {2, -1, -1};
static const int t62_feature[3] = {52, -2, -2};
static const double t62_threshold[3] = {0.27633124589920044, -2, -2};
static const double t62_probabilities[6] = {0.52941176470588236, 0.47058823529411764, 1, 0, 0.20000000000000001, 0.80000000000000004};
static const int t63_children_left[1] = {-1};
static const int t63_children_right[1] = {-1};
static const int t63_feature[1] = {-2};
static const double t63_threshold[1] = {-2};
static const double t63_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t64_children_left[1] = {-1};
static const int t64_children_right[1] = {-1};
static const int t64_feature[1] = {-2};
static const double t64_threshold[1] = {-2};
static const double t64_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t65_children_left[3] = {1, -1, -1};
static const int t65_children_right[3] = {2, -1, -1};
static const int t65_feature[3] = {64, -2, -2};
static const double t65_threshold[3] = {0.13111082464456558, -2, -2};
static const double t65_probabilities[6] = {0.6470588235294118, 0.35294117647058826, 1, 0, 0.14285714285714285, 0.8571428571428571};
static const int t66_children_left[3] = {1, -1, -1};
static const int t66_children_right[3] = {2, -1, -1};
static const int t66_feature[3] = {15, -2, -2};
static const double t66_threshold[3] = {0.015871790237724781, -2, -2};
static const double t66_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t67_children_left[1] = {-1};
static const int t67_children_right[1] = {-1};
static const int t67_feature[1] = {-2};
static const double t67_threshold[1] = {-2};
static const double t67_probabilities[2] = {0.35294117647058826, 0.6470588235294118};
static const int t68_children_left[3] = {1, -1, -1};
static const int t68_children_right[3] = {2, -1, -1};
static const int t68_feature[3] = {39, -2, -2};
static const double t68_threshold[3] = {0.1007976271212101, -2, -2};
static const double t68_probabilities[6] = {0.6470588235294118, 0.35294117647058826, 1, 0, 0.14285714285714285, 0.8571428571428571};
static const int t69_children_left[3] = {1, -1, -1};
static const int t69_children_right[3] = {2, -1, -1};
static const int t69_feature[3] = {77, -2, -2};
static const double t69_threshold[3] = {0.15951848030090332, -2, -2};
static const double t69_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 0.10000000000000001, 0.90000000000000002, 1, 0};
static const int t70_children_left[3] = {1, -1, -1};
static const int t70_children_right[3] = {2, -1, -1};
static const int t70_feature[3] = {37, -2, -2};
static const double t70_threshold[3] = {0.12535666674375534, -2, -2};
static const double t70_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.10000000000000001, 0.90000000000000002};
static const int t71_children_left[3] = {1, -1, -1};
static const int t71_children_right[3] = {2, -1, -1};
static const int t71_feature[3] = {42, -2, -2};
static const double t71_threshold[3] = {0.37580344080924988, -2, -2};
static const double t71_probabilities[6] = {0.6470588235294118, 0.35294117647058826, 1, 0, 0.25, 0.75};
static const int t72_children_left[3] = {1, -1, -1};
static const int t72_children_right[3] = {2, -1, -1};
static const int t72_feature[3] = {37, -2, -2};
static const double t72_threshold[3] = {0.11591634899377823, -2, -2};
static const double t72_probabilities[6] = {0.41176470588235292, 0.58823529411764708, 1, 0, 0.090909090909090912, 0.90909090909090906};
static const int t73_children_left[3] = {1, -1, -1};
static const int t73_children_right[3] = {2, -1, -1};
static const int t73_feature[3] = {18, -2, -2};
static const double t73_threshold[3] = {0.072290554642677307, -2, -2};
static const double t73_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 0.29999999999999999, 0.69999999999999996, 1, 0};
static const int t74_children_left[3] = {1, -1, -1};
static const int t74_children_right[3] = {2, -1, -1};
static const int t74_feature[3] = {15, -2, -2};
static const double t74_threshold[3] = {0.013514268677681684, -2, -2};
static const double t74_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t75_children_left[3] = {1, -1, -1};
static const int t75_children_right[3] = {2, -1, -1};
static const int t75_feature[3] = {1, -2, -2};
static const double t75_threshold[3] = {6.8858163356781006, -2, -2};
static const double t75_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.18181818181818182, 0.81818181818181823};
static const int t76_children_left[1] = {-1};
static const int t76_children_right[1] = {-1};
static const int t76_feature[1] = {-2};
static const double t76_threshold[1] = {-2};
static const double t76_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t77_children_left[1] = {-1};
static const int t77_children_right[1] = {-1};
static const int t77_feature[1] = {-2};
static const double t77_threshold[1] = {-2};
static const double t77_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t78_children_left[3] = {1, -1, -1};
static const int t78_children_right[3] = {2, -1, -1};
static const int t78_feature[3] = {14, -2, -2};
static const double t78_threshold[3] = {0.16563672572374344, -2, -2};
static const double t78_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.10000000000000001, 0.90000000000000002};
static const int t79_children_left[3] = {1, -1, -1};
static const int t79_children_right[3] = {2, -1, -1};
static const int t79_feature[3] = {15, -2, -2};
static const double t79_threshold[3] = {0.011717014946043491, -2, -2};
static const double t79_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t80_children_left[1] = {-1};
static const int t80_children_right[1] = {-1};
static const int t80_feature[1] = {-2};
static const double t80_threshold[1] = {-2};
static const double t80_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t81_children_left[1] = {-1};
static const int t81_children_right[1] = {-1};
static const int t81_feature[1] = {-2};
static const double t81_threshold[1] = {-2};
static const double t81_probabilities[2] = {0.29411764705882354, 0.70588235294117652};
static const int t82_children_left[1] = {-1};
static const int t82_children_right[1] = {-1};
static const int t82_feature[1] = {-2};
static const double t82_threshold[1] = {-2};
static const double t82_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t83_children_left[3] = {1, -1, -1};
static const int t83_children_right[3] = {2, -1, -1};
static const int t83_feature[3] = {64, -2, -2};
static const double t83_threshold[3] = {0.13111082464456558, -2, -2};
static const double t83_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t84_children_left[3] = {1, -1, -1};
static const int t84_children_right[3] = {2, -1, -1};
static const int t84_feature[3] = {10, -2, -2};
static const double t84_threshold[3] = {0.11391358450055122, -2, -2};
static const double t84_probabilities[6] = {0.41176470588235292, 0.58823529411764708, 0.090909090909090912, 0.90909090909090906, 1, 0};
static const int t85_children_left[1] = {-1};
static const int t85_children_right[1] = {-1};
static const int t85_feature[1] = {-2};
static const double t85_threshold[1] = {-2};
static const double t85_probabilities[2] = {0.35294117647058826, 0.6470588235294118};
static const int t86_children_left[3] = {1, -1, -1};
static const int t86_children_right[3] = {2, -1, -1};
static const int t86_feature[3] = {76, -2, -2};
static const double t86_threshold[3] = {-0.28876599669456482, -2, -2};
static const double t86_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 1, 0, 0.10000000000000001, 0.90000000000000002};
static const int t87_children_left[3] = {1, -1, -1};
static const int t87_children_right[3] = {2, -1, -1};
static const int t87_feature[3] = {51, -2, -2};
static const double t87_threshold[3] = {7.1824157238006592, -2, -2};
static const double t87_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t88_children_left[1] = {-1};
static const int t88_children_right[1] = {-1};
static const int t88_feature[1] = {-2};
static const double t88_threshold[1] = {-2};
static const double t88_probabilities[2] = {0.47058823529411764, 0.52941176470588236};
static const int t89_children_left[3] = {1, -1, -1};
static const int t89_children_right[3] = {2, -1, -1};
static const int t89_feature[3] = {34, -2, -2};
static const double t89_threshold[3] = {0.99477678537368774, -2, -2};
static const double t89_probabilities[6] = {0.47058823529411764, 0.52941176470588236, 0.10000000000000001, 0.90000000000000002, 1, 0};
static const int t90_children_left[3] = {1, -1, -1};
static const int t90_children_right[3] = {2, -1, -1};
static const int t90_feature[3] = {77, -2, -2};
static const double t90_threshold[3] = {0.15077248215675354, -2, -2};
static const double t90_probabilities[6] = {0.35294117647058826, 0.6470588235294118, 0.083333333333333329, 0.91666666666666663, 1, 0};
static const int t91_children_left[1] = {-1};
static const int t91_children_right[1] = {-1};
static const int t91_feature[1] = {-2};
static const double t91_threshold[1] = {-2};
static const double t91_probabilities[2] = {0.41176470588235292, 0.58823529411764708};
static const int t92_children_left[3] = {1, -1, -1};
static const int t92_children_right[3] = {2, -1, -1};
static const int t92_feature[3] = {68, -2, -2};
static const double t92_threshold[3] = {0.056391919031739235, -2, -2};
static const double t92_probabilities[6] = {0.52941176470588236, 0.47058823529411764, 0.1111111111111111, 0.88888888888888884, 1, 0};
static const int t93_children_left[3] = {1, -1, -1};
static const int t93_children_right[3] = {2, -1, -1};
static const int t93_feature[3] = {49, -2, -2};
static const double t93_threshold[3] = {-5.8686399459838867, -2, -2};
static const double t93_probabilities[6] = {0.58823529411764708, 0.41176470588235292, 1, 0, 0.125, 0.875};
static const int t94_children_left[3] = {1, -1, -1};
static const int t94_children_right[3] = {2, -1, -1};
static const int t94_feature[3] = {12, -2, -2};
static const double t94_threshold[3] = {0.21377867460250854, -2, -2};
static const double t94_probabilities[6] = {0.70588235294117652, 0.29411764705882354, 1, 0, 0.2857142857142857, 0.7142857142857143};
static const int t95_children_left[3] = {1, -1, -1};
static const int t95_children_right[3] = {2, -1, -1};
static const int t95_feature[3] = {68, -2, -2};
static const double t95_threshold[3] = {0.056391919031739235, -2, -2};
static const double t95_probabilities[6] = {0.6470588235294118, 0.35294117647058826, 0.14285714285714285, 0.8571428571428571, 1, 0};
int csi_rf_predict(const float *input, double *proba) {
  for (int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]=0;
  { int node=0;
    while(t0_children_left[node]>=0) { int input_index=feature_mapping[t0_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t0_threshold[node])?t0_children_left[node]:t0_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t0_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t1_children_left[node]>=0) { int input_index=feature_mapping[t1_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t1_threshold[node])?t1_children_left[node]:t1_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t1_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t2_children_left[node]>=0) { int input_index=feature_mapping[t2_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t2_threshold[node])?t2_children_left[node]:t2_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t2_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t3_children_left[node]>=0) { int input_index=feature_mapping[t3_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t3_threshold[node])?t3_children_left[node]:t3_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t3_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t4_children_left[node]>=0) { int input_index=feature_mapping[t4_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t4_threshold[node])?t4_children_left[node]:t4_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t4_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t5_children_left[node]>=0) { int input_index=feature_mapping[t5_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t5_threshold[node])?t5_children_left[node]:t5_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t5_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t6_children_left[node]>=0) { int input_index=feature_mapping[t6_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t6_threshold[node])?t6_children_left[node]:t6_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t6_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t7_children_left[node]>=0) { int input_index=feature_mapping[t7_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t7_threshold[node])?t7_children_left[node]:t7_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t7_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t8_children_left[node]>=0) { int input_index=feature_mapping[t8_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t8_threshold[node])?t8_children_left[node]:t8_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t8_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t9_children_left[node]>=0) { int input_index=feature_mapping[t9_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t9_threshold[node])?t9_children_left[node]:t9_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t9_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t10_children_left[node]>=0) { int input_index=feature_mapping[t10_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t10_threshold[node])?t10_children_left[node]:t10_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t10_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t11_children_left[node]>=0) { int input_index=feature_mapping[t11_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t11_threshold[node])?t11_children_left[node]:t11_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t11_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t12_children_left[node]>=0) { int input_index=feature_mapping[t12_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t12_threshold[node])?t12_children_left[node]:t12_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t12_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t13_children_left[node]>=0) { int input_index=feature_mapping[t13_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t13_threshold[node])?t13_children_left[node]:t13_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t13_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t14_children_left[node]>=0) { int input_index=feature_mapping[t14_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t14_threshold[node])?t14_children_left[node]:t14_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t14_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t15_children_left[node]>=0) { int input_index=feature_mapping[t15_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t15_threshold[node])?t15_children_left[node]:t15_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t15_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t16_children_left[node]>=0) { int input_index=feature_mapping[t16_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t16_threshold[node])?t16_children_left[node]:t16_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t16_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t17_children_left[node]>=0) { int input_index=feature_mapping[t17_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t17_threshold[node])?t17_children_left[node]:t17_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t17_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t18_children_left[node]>=0) { int input_index=feature_mapping[t18_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t18_threshold[node])?t18_children_left[node]:t18_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t18_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t19_children_left[node]>=0) { int input_index=feature_mapping[t19_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t19_threshold[node])?t19_children_left[node]:t19_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t19_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t20_children_left[node]>=0) { int input_index=feature_mapping[t20_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t20_threshold[node])?t20_children_left[node]:t20_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t20_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t21_children_left[node]>=0) { int input_index=feature_mapping[t21_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t21_threshold[node])?t21_children_left[node]:t21_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t21_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t22_children_left[node]>=0) { int input_index=feature_mapping[t22_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t22_threshold[node])?t22_children_left[node]:t22_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t22_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t23_children_left[node]>=0) { int input_index=feature_mapping[t23_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t23_threshold[node])?t23_children_left[node]:t23_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t23_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t24_children_left[node]>=0) { int input_index=feature_mapping[t24_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t24_threshold[node])?t24_children_left[node]:t24_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t24_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t25_children_left[node]>=0) { int input_index=feature_mapping[t25_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t25_threshold[node])?t25_children_left[node]:t25_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t25_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t26_children_left[node]>=0) { int input_index=feature_mapping[t26_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t26_threshold[node])?t26_children_left[node]:t26_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t26_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t27_children_left[node]>=0) { int input_index=feature_mapping[t27_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t27_threshold[node])?t27_children_left[node]:t27_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t27_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t28_children_left[node]>=0) { int input_index=feature_mapping[t28_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t28_threshold[node])?t28_children_left[node]:t28_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t28_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t29_children_left[node]>=0) { int input_index=feature_mapping[t29_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t29_threshold[node])?t29_children_left[node]:t29_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t29_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t30_children_left[node]>=0) { int input_index=feature_mapping[t30_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t30_threshold[node])?t30_children_left[node]:t30_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t30_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t31_children_left[node]>=0) { int input_index=feature_mapping[t31_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t31_threshold[node])?t31_children_left[node]:t31_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t31_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t32_children_left[node]>=0) { int input_index=feature_mapping[t32_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t32_threshold[node])?t32_children_left[node]:t32_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t32_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t33_children_left[node]>=0) { int input_index=feature_mapping[t33_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t33_threshold[node])?t33_children_left[node]:t33_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t33_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t34_children_left[node]>=0) { int input_index=feature_mapping[t34_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t34_threshold[node])?t34_children_left[node]:t34_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t34_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t35_children_left[node]>=0) { int input_index=feature_mapping[t35_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t35_threshold[node])?t35_children_left[node]:t35_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t35_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t36_children_left[node]>=0) { int input_index=feature_mapping[t36_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t36_threshold[node])?t36_children_left[node]:t36_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t36_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t37_children_left[node]>=0) { int input_index=feature_mapping[t37_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t37_threshold[node])?t37_children_left[node]:t37_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t37_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t38_children_left[node]>=0) { int input_index=feature_mapping[t38_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t38_threshold[node])?t38_children_left[node]:t38_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t38_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t39_children_left[node]>=0) { int input_index=feature_mapping[t39_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t39_threshold[node])?t39_children_left[node]:t39_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t39_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t40_children_left[node]>=0) { int input_index=feature_mapping[t40_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t40_threshold[node])?t40_children_left[node]:t40_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t40_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t41_children_left[node]>=0) { int input_index=feature_mapping[t41_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t41_threshold[node])?t41_children_left[node]:t41_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t41_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t42_children_left[node]>=0) { int input_index=feature_mapping[t42_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t42_threshold[node])?t42_children_left[node]:t42_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t42_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t43_children_left[node]>=0) { int input_index=feature_mapping[t43_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t43_threshold[node])?t43_children_left[node]:t43_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t43_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t44_children_left[node]>=0) { int input_index=feature_mapping[t44_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t44_threshold[node])?t44_children_left[node]:t44_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t44_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t45_children_left[node]>=0) { int input_index=feature_mapping[t45_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t45_threshold[node])?t45_children_left[node]:t45_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t45_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t46_children_left[node]>=0) { int input_index=feature_mapping[t46_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t46_threshold[node])?t46_children_left[node]:t46_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t46_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t47_children_left[node]>=0) { int input_index=feature_mapping[t47_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t47_threshold[node])?t47_children_left[node]:t47_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t47_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t48_children_left[node]>=0) { int input_index=feature_mapping[t48_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t48_threshold[node])?t48_children_left[node]:t48_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t48_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t49_children_left[node]>=0) { int input_index=feature_mapping[t49_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t49_threshold[node])?t49_children_left[node]:t49_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t49_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t50_children_left[node]>=0) { int input_index=feature_mapping[t50_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t50_threshold[node])?t50_children_left[node]:t50_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t50_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t51_children_left[node]>=0) { int input_index=feature_mapping[t51_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t51_threshold[node])?t51_children_left[node]:t51_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t51_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t52_children_left[node]>=0) { int input_index=feature_mapping[t52_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t52_threshold[node])?t52_children_left[node]:t52_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t52_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t53_children_left[node]>=0) { int input_index=feature_mapping[t53_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t53_threshold[node])?t53_children_left[node]:t53_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t53_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t54_children_left[node]>=0) { int input_index=feature_mapping[t54_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t54_threshold[node])?t54_children_left[node]:t54_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t54_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t55_children_left[node]>=0) { int input_index=feature_mapping[t55_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t55_threshold[node])?t55_children_left[node]:t55_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t55_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t56_children_left[node]>=0) { int input_index=feature_mapping[t56_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t56_threshold[node])?t56_children_left[node]:t56_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t56_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t57_children_left[node]>=0) { int input_index=feature_mapping[t57_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t57_threshold[node])?t57_children_left[node]:t57_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t57_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t58_children_left[node]>=0) { int input_index=feature_mapping[t58_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t58_threshold[node])?t58_children_left[node]:t58_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t58_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t59_children_left[node]>=0) { int input_index=feature_mapping[t59_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t59_threshold[node])?t59_children_left[node]:t59_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t59_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t60_children_left[node]>=0) { int input_index=feature_mapping[t60_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t60_threshold[node])?t60_children_left[node]:t60_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t60_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t61_children_left[node]>=0) { int input_index=feature_mapping[t61_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t61_threshold[node])?t61_children_left[node]:t61_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t61_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t62_children_left[node]>=0) { int input_index=feature_mapping[t62_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t62_threshold[node])?t62_children_left[node]:t62_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t62_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t63_children_left[node]>=0) { int input_index=feature_mapping[t63_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t63_threshold[node])?t63_children_left[node]:t63_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t63_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t64_children_left[node]>=0) { int input_index=feature_mapping[t64_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t64_threshold[node])?t64_children_left[node]:t64_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t64_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t65_children_left[node]>=0) { int input_index=feature_mapping[t65_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t65_threshold[node])?t65_children_left[node]:t65_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t65_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t66_children_left[node]>=0) { int input_index=feature_mapping[t66_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t66_threshold[node])?t66_children_left[node]:t66_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t66_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t67_children_left[node]>=0) { int input_index=feature_mapping[t67_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t67_threshold[node])?t67_children_left[node]:t67_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t67_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t68_children_left[node]>=0) { int input_index=feature_mapping[t68_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t68_threshold[node])?t68_children_left[node]:t68_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t68_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t69_children_left[node]>=0) { int input_index=feature_mapping[t69_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t69_threshold[node])?t69_children_left[node]:t69_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t69_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t70_children_left[node]>=0) { int input_index=feature_mapping[t70_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t70_threshold[node])?t70_children_left[node]:t70_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t70_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t71_children_left[node]>=0) { int input_index=feature_mapping[t71_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t71_threshold[node])?t71_children_left[node]:t71_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t71_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t72_children_left[node]>=0) { int input_index=feature_mapping[t72_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t72_threshold[node])?t72_children_left[node]:t72_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t72_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t73_children_left[node]>=0) { int input_index=feature_mapping[t73_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t73_threshold[node])?t73_children_left[node]:t73_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t73_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t74_children_left[node]>=0) { int input_index=feature_mapping[t74_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t74_threshold[node])?t74_children_left[node]:t74_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t74_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t75_children_left[node]>=0) { int input_index=feature_mapping[t75_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t75_threshold[node])?t75_children_left[node]:t75_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t75_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t76_children_left[node]>=0) { int input_index=feature_mapping[t76_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t76_threshold[node])?t76_children_left[node]:t76_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t76_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t77_children_left[node]>=0) { int input_index=feature_mapping[t77_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t77_threshold[node])?t77_children_left[node]:t77_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t77_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t78_children_left[node]>=0) { int input_index=feature_mapping[t78_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t78_threshold[node])?t78_children_left[node]:t78_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t78_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t79_children_left[node]>=0) { int input_index=feature_mapping[t79_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t79_threshold[node])?t79_children_left[node]:t79_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t79_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t80_children_left[node]>=0) { int input_index=feature_mapping[t80_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t80_threshold[node])?t80_children_left[node]:t80_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t80_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t81_children_left[node]>=0) { int input_index=feature_mapping[t81_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t81_threshold[node])?t81_children_left[node]:t81_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t81_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t82_children_left[node]>=0) { int input_index=feature_mapping[t82_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t82_threshold[node])?t82_children_left[node]:t82_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t82_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t83_children_left[node]>=0) { int input_index=feature_mapping[t83_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t83_threshold[node])?t83_children_left[node]:t83_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t83_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t84_children_left[node]>=0) { int input_index=feature_mapping[t84_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t84_threshold[node])?t84_children_left[node]:t84_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t84_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t85_children_left[node]>=0) { int input_index=feature_mapping[t85_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t85_threshold[node])?t85_children_left[node]:t85_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t85_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t86_children_left[node]>=0) { int input_index=feature_mapping[t86_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t86_threshold[node])?t86_children_left[node]:t86_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t86_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t87_children_left[node]>=0) { int input_index=feature_mapping[t87_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t87_threshold[node])?t87_children_left[node]:t87_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t87_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t88_children_left[node]>=0) { int input_index=feature_mapping[t88_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t88_threshold[node])?t88_children_left[node]:t88_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t88_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t89_children_left[node]>=0) { int input_index=feature_mapping[t89_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t89_threshold[node])?t89_children_left[node]:t89_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t89_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t90_children_left[node]>=0) { int input_index=feature_mapping[t90_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t90_threshold[node])?t90_children_left[node]:t90_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t90_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t91_children_left[node]>=0) { int input_index=feature_mapping[t91_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t91_threshold[node])?t91_children_left[node]:t91_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t91_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t92_children_left[node]>=0) { int input_index=feature_mapping[t92_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t92_threshold[node])?t92_children_left[node]:t92_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t92_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t93_children_left[node]>=0) { int input_index=feature_mapping[t93_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t93_threshold[node])?t93_children_left[node]:t93_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t93_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t94_children_left[node]>=0) { int input_index=feature_mapping[t94_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t94_threshold[node])?t94_children_left[node]:t94_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t94_probabilities[node*CSI_RF_N_CLASSES+c]; }
  { int node=0;
    while(t95_children_left[node]>=0) { int input_index=feature_mapping[t95_feature[node]]; float x=input[input_index]; if(!isfinite(x)) x=(float)imputation[input_index]; node=((double)x<=t95_threshold[node])?t95_children_left[node]:t95_children_right[node]; }
    for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]+=t95_probabilities[node*CSI_RF_N_CLASSES+c]; }
  for(int c=0;c<CSI_RF_N_CLASSES;++c) proba[c]/=96.0;
  int best=0;for(int c=1;c<CSI_RF_N_CLASSES;++c) if(proba[c]>proba[best]) best=c;return best;
}
