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
    
    double num = sum_xy - (sum_x*sum_y)/n;
    double den = sqrt( (sum_xx - pow(sum_x, 2)/n)*(sum_yy - pow(sum_y, 2)/n) );
    
    return (fabs(den) > 1e-9) ? num / den : 0.0;
}

void calculate_correlation(time_t time_now) {
    struct stat st = {0};
    // Ensure data directory exists
    if (stat("data", &st) == -1) {
        mkdir("data", 0755);
        printf("DEBUG: Created data directory\n");
    }

    // Ensure moving average directory exists
    if (stat("data/corr", &st) == -1) {
        mkdir("data/corr", 0755);
        printf("DEBUG: Created data/corr directory\n");
    }

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
        int correlation_idx = 0;
        
        for(int j = 0; j < 8; j++) {
            if(j == i) {
                correlations[correlation_idx++] = 1.0; // Self-correlation
                correlation_idx++;
                continue;
            }

            pthread_mutex_lock(&symbol_histories[j].mutex);
            if(symbol_histories[j].movingAvg_count >= 8) {
                double x[8], y[8];
                
                // Populate x and y arrays
                for(int k = 0; k < 8; k++) {
                    int idx_i = (symbol_histories[i].movingAvg_index - 8 + k + 8) % 8;
                    int idx_j = (symbol_histories[j].movingAvg_index - 8 + k + 8) % 8;
                    x[k] = symbol_histories[i].movingAvg_history[idx_i];
                    y[k] = symbol_histories[j].movingAvg_history[idx_j];
                }

                double correlation = pearson_correlation(x, y, 8);
                printf("DEBUG: Correlation between %s and %s: %.4f\n", symbols[i], symbols[j], correlation);
                correlations[correlation_idx++] = correlation;

                if(correlation > max_correlation) {
                    max_correlation = correlation;
                    strncpy(max_symbol, symbols[j], sizeof(max_symbol) - 1);
                    max_symbol[sizeof(max_symbol) - 1] = '\0'; // Ensure null-termination
                }
            } else {
                correlations[correlation_idx++] = 0.0; // Not enough data
            }
            pthread_mutex_unlock(&symbol_histories[j].mutex);
            correlation_idx++;
        }

        // Write correlation to log
        printf("DEBUG: Max correlation for %s: %.4f with %s\n", symbols[i], max_correlation, max_symbol);
        printf("DEBUG: Writing to file\n");
        char corr_filename[128];
        snprintf(corr_filename, sizeof(corr_filename), "data/corr/%s.log", symbols[i]);
        FILE* file = fopen(corr_filename, "a");
        if (file) {
            int bytes_written = fprintf(file, "%llu,%s,%.4f", (unsigned long long)time_now, max_symbol, max_correlation);
            
            // Append all correlations
            for (int k = 0; k < 8; k++) {
                fprintf(file, ",%.4f", correlations[k]);
            }
            fprintf(file, "\n");
            fflush(file);
            fclose(file);

            printf("DEBUG: Successfully wrote %d bytes to CORR file for %s\n", bytes_written, symbols[i]);
        
            // Verify the file was actually written
            struct stat file_stat;
            if (stat(corr_filename, &file_stat) == 0) {
                printf("DEBUG: File %s exists, size: %ld bytes\n", corr_filename, file_stat.st_size);
            }
        } else {
            printf("DEBUG: Failed to open file %s: %s\n", corr_filename, strerror(errno));
        }

        pthread_mutex_unlock(&symbol_histories[i].mutex);
    }    
}