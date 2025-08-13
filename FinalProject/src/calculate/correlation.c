#include "correlation.h"
#include "../utils/utils.h"

double pearson_correlation(double* x, double* y, int n) {
    if (n < 2) return 0.0;

    double sum_x = 0.0, sum_y = 0.0;
    double sum_xx = 0.0, sum_yy = 0.0, sum_xy = 0.0;
    
    for (int i = 0; i < n; i++) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xx += x[i] * x[i];
        sum_yy += y[i] * y[i];
        sum_xy += x[i] * y[i];
    }
    
    double num = sum_xy - (sum_x*sum_y) / n;
    double den = sqrt((sum_xx - pow(sum_x, 2) / n) * (sum_yy - pow(sum_y, 2) / n));
    
    return (fabs(den) > 1e-9) ? num / den : 0.0;
}

void calculate_correlation(time_t time_now) {
    struct stat st = {0};
    if (stat("data", &st) == -1) mkdir("data", 0755);
    if (stat("data/corr", &st) == -1) mkdir("data/corr", 0755);

    printf("DEBUG: Calculating correlations at %s", ctime(&time_now));
    for(int i = 0; i < 8; i++) {
        pthread_mutex_lock(&symbol_histories[i].mutex);

        if(symbol_histories[i].movingAvg_count < 8) {
            pthread_mutex_unlock(&symbol_histories[i].mutex);
            continue;
        }

        double correlations[8] = {0};
        char max_symbol[32] = "N/A";
        double max_correlation = -2.0;
        
        for(int j = 0; j < 8; j++) {
            if(j == i) {
                correlations[j] = 1.0; // Self-correlation
                continue;
            }

            pthread_mutex_lock(&symbol_histories[j].mutex);
            if(symbol_histories[j].movingAvg_count >= 8) {
                double x[8], y[8];
                
                // Extract last 8 values in chronological order
                for(int k = 0; k < 8; k++) {
                    int idx_i = (symbol_histories[i].movingAvg_index - 8 + k + 8) % 8;
                    int idx_j = (symbol_histories[j].movingAvg_index - 8 + k + 8) % 8;
                    x[k] = symbol_histories[i].movingAvg_history[idx_i];
                    y[k] = symbol_histories[j].movingAvg_history[idx_j];
                }

                double correlation = pearson_correlation(x, y, 8);
                correlations[j] = correlation;

                if(j != i && correlation > max_correlation) {
                    max_correlation = correlation;
                    strncpy(max_symbol, symbols[j], sizeof(max_symbol) - 1);
                }
            } else {
                correlations[j] = 0.0;
            }
            pthread_mutex_unlock(&symbol_histories[j].mutex);
        }

        // Write to file with all correlations
        char corr_filename[128];
        snprintf(corr_filename, sizeof(corr_filename), "data/corr/%s.log", symbols[i]);
        FILE* file = fopen(corr_filename, "a");
        if (file) {
            fprintf(file, "%llu,%s,%.4f", (unsigned long long)time_now, max_symbol, max_correlation);
            for (int k = 0; k < 8; k++) {
                fprintf(file, ",%.4f", correlations[k]);
            }
            fprintf(file, "\n");
            fflush(file);
            fclose(file);
        }

        pthread_mutex_unlock(&symbol_histories[i].mutex);
    }    
}