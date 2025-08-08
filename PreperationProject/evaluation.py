from concurrent.futures import ProcessPoolExecutor
import matplotlib.pyplot as plt
import pandas as pd
import subprocess
import time
import re
import os

C_PROGRAM = "./ProdConTestEX"           # Compiled C program
OUTPUT_DIR = "results"                  # Directory to store results
NUM_RUNS = 5                            # Number of runs per configuration for stability
QUEUE_SIZES = [10]                      # Different queue sizes to test
PRODUCER_COUNTS = [10, 20]              # Producer counts to test
CONSUMER_COUNTS = list(range(1, 21))    # Consumer counts from 1 to 20

def compile_program():
    print("Compiling the C program...")
    result = subprocess.run(["gcc", "-o", C_PROGRAM, C_PROGRAM + ".c", "-lpthread", "-lm"], 
                          capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Compilation failed: {result.stderr}")
        exit(1)
    print("Compilation successful")

def run_test(producer_count, consumer_count, queue_size, run_index):
    print(f"Running test with {producer_count} producers, {consumer_count} consumers, queue size {queue_size}, run {run_index+1}/{NUM_RUNS}")
    
    cmd = [C_PROGRAM, str(producer_count), str(consumer_count), str(queue_size)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"Test failed: {result.stderr}")
        return None
    
    # Extract metrics using regex
    metrics = {}
    
    # Queue Wait Time Statistics
    avg_wait_match = re.search(r"Average wait time: ([\d.]+) microseconds", result.stdout)
    if avg_wait_match:
        metrics['avg_wait_time'] = float(avg_wait_match.group(1))
    
    min_wait_match = re.search(r"Minimum wait time: (\d+) microseconds", result.stdout)
    if min_wait_match:
        metrics['min_wait_time'] = int(min_wait_match.group(1))
    
    max_wait_match = re.search(r"Maximum wait time: (\d+) microseconds", result.stdout)
    if max_wait_match:
        metrics['max_wait_time'] = int(max_wait_match.group(1))
    
    # Mutex Contention Statistics
    mutex_contests_match = re.search(r"Total mutex contests: (\d+)", result.stdout)
    if mutex_contests_match:
        metrics['mutex_contests'] = int(mutex_contests_match.group(1))
    
    avg_mutex_wait_match = re.search(r"Average mutex wait time: ([\d.]+) microseconds", result.stdout)
    if avg_mutex_wait_match:
        metrics['avg_mutex_wait'] = float(avg_mutex_wait_match.group(1))
    
    max_mutex_wait_match = re.search(r"Maximum mutex wait time: (\d+) microseconds", result.stdout)
    if max_mutex_wait_match:
        metrics['max_mutex_wait'] = int(max_mutex_wait_match.group(1))
    
    # Queue State Statistics
    empty_match = re.search(r"Empty queue encounters: (\d+)", result.stdout)
    if empty_match:
        metrics['empty_encounters'] = int(empty_match.group(1))
    
    full_match = re.search(r"Full queue encounters: (\d+)", result.stdout)
    if full_match:
        metrics['full_encounters'] = int(full_match.group(1))
    
    # Store configuration
    metrics['producers'] = producer_count
    metrics['consumers'] = consumer_count
    metrics['queue_size'] = queue_size
    metrics['run'] = run_index
    
    return metrics

def run_test_with_retries(config):
    producer_count, consumer_count, queue_size = config
    all_metrics = []
    
    for i in range(NUM_RUNS):
        metrics = run_test(producer_count, consumer_count, queue_size, i)
        if metrics:
            all_metrics.append(metrics)
    
    if not all_metrics:
        return None
    
    # Average the metrics across runs
    avg_metrics = {}
    for key in all_metrics[0].keys():
        if key != 'run':  # Don't average the run index
            values = [m[key] for m in all_metrics if key in m]
            if values:
                avg_metrics[key] = sum(values) / len(values)
    
    # Store configuration
    avg_metrics['producers'] = producer_count
    avg_metrics['consumers'] = consumer_count
    avg_metrics['queue_size'] = queue_size
    
    return avg_metrics

def run_all_tests():
    results = []
    configs = []
    
    # Generate all test configurations
    for p in PRODUCER_COUNTS:
        for c in CONSUMER_COUNTS:
            for q in QUEUE_SIZES:
                configs.append((p, c, q))
    
    # Run tests in parallel using process pool
    with ProcessPoolExecutor(max_workers=os.cpu_count()) as executor:
        for result in executor.map(run_test_with_retries, configs):
            if result:
                results.append(result)
    
    return pd.DataFrame(results)

def create_visualizations(df):
    # For each queue size and producer count
    for q in QUEUE_SIZES:
        for p in PRODUCER_COUNTS:
            subset = df[(df['queue_size'] == q) & (df['producers'] == p)]
            
            if subset.empty:
                continue
                
            # Plot of consumers vs average wait time
            plt.figure(figsize=(10, 6))
            plt.plot(subset['consumers'], subset['avg_wait_time'], marker='o')
            plt.xlabel('Number of Consumers')
            plt.ylabel('Average Wait Time (microseconds)')
            plt.title(f'Average Queue Wait Time (P={p}, Q={q})')
            plt.grid(True)
            plt.savefig(f"{OUTPUT_DIR}/avg_wait_p{p}_q{q}.png")
            plt.close()
            
            # Plot of consumers vs mutex contention
            plt.figure(figsize=(10, 6))
            plt.plot(subset['consumers'], subset['avg_mutex_wait'], marker='o', color='orange')
            plt.xlabel('Number of Consumers')
            plt.ylabel('Average Mutex Wait Time (microseconds)')
            plt.title(f'Mutex Contention (P={p}, Q={q})')
            plt.grid(True)
            plt.savefig(f"{OUTPUT_DIR}/mutex_wait_p{p}_q{q}.png")
            plt.close()
            
            # Plot of consumers vs empty/full encounters
            plt.figure(figsize=(10, 6))
            plt.plot(subset['consumers'], subset['empty_encounters'], marker='o', label='Empty Queue')
            plt.plot(subset['consumers'], subset['full_encounters'], marker='x', label='Full Queue')
            plt.xlabel('Number of Consumers')
            plt.ylabel('Number of Encounters')
            plt.title(f'Queue State Encounters (P={p}, Q={q})')
            plt.legend()
            plt.grid(True)
            plt.savefig(f"{OUTPUT_DIR}/queue_state_p{p}_q{q}.png")
            plt.close()
    
    # Create combined visualizations
    for p in PRODUCER_COUNTS:
        plt.figure(figsize=(12, 8))
        for q in QUEUE_SIZES:
            subset = df[(df['queue_size'] == q) & (df['producers'] == p)]
            if not subset.empty:
                plt.plot(subset['consumers'], subset['avg_wait_time'], marker='o', label=f'Queue Size={q}')
        
        plt.xlabel('Number of Consumers')
        plt.ylabel('Average Wait Time (microseconds)')
        plt.title(f'Average Queue Wait Time for {p} Producers')
        plt.legend()
        plt.grid(True)
        plt.savefig(f"{OUTPUT_DIR}/combined_wait_time_p{p}.png")
        plt.close()

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    compile_program()
    
    # Run all tests
    print("Starting test runs...")
    start_time = time.time()
    results_df = run_all_tests()
    elapsed = time.time() - start_time
    print(f"All tests completed in {elapsed:.1f} seconds")
    
    # Create visualizations
    print("Creating visualizations...")
    create_visualizations(results_df)
    
    print(f"\nAnalysis complete. Results saved to {OUTPUT_DIR} directory.")

if __name__ == "__main__":
    main()