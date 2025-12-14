"""
Performance benchmark tracking for GPU acceleration.

This module provides utilities for tracking and comparing performance
of CPU vs GPU implementations across commits.
"""

import json
import time
from pathlib import Path
import subprocess
from typing import Dict, Optional


class PerformanceRegression(Exception):
    """Raised when performance drops below acceptable threshold."""
    pass


class BenchmarkTracker:
    """
    Track benchmark results across commits.
    
    This class helps monitor GPU acceleration performance and detect
    regressions early.
    """
    
    def __init__(self, results_file: str = "benchmark_results.json"):
        """
        Initialize benchmark tracker.
        
        Args:
            results_file: Path to JSON file for storing results
        """
        self.results_file = Path(results_file)
        self.results = self._load_results()
    
    def _load_results(self) -> Dict:
        """Load existing benchmark results from file."""
        if self.results_file.exists():
            try:
                with open(self.results_file, 'r') as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError):
                return {}
        return {}
    
    def _save_results(self):
        """Save benchmark results to file."""
        try:
            with open(self.results_file, 'w') as f:
                json.dump(self.results, f, indent=2)
        except IOError as e:
            print(f"Warning: Failed to save benchmark results: {e}")
    
    def record(self, test_name: str, cpu_time: float, gpu_time: float, 
               commit_hash: Optional[str] = None):
        """
        Record benchmark results for a test.
        
        Args:
            test_name: Name of the test/benchmark
            cpu_time: CPU execution time in seconds
            gpu_time: GPU execution time in seconds
            commit_hash: Git commit hash (auto-detected if None)
            
        Raises:
            PerformanceRegression: If speedup drops below baseline
        """
        if commit_hash is None:
            commit_hash = self._get_git_commit()
        
        speedup = cpu_time / gpu_time if gpu_time > 0 else 0
        
        # Store result
        if test_name not in self.results:
            self.results[test_name] = []
        
        self.results[test_name].append({
            "cpu_time": cpu_time,
            "gpu_time": gpu_time,
            "speedup": speedup,
            "commit": commit_hash,
            "timestamp": time.time()
        })
        
        self._save_results()
        
        # Check for regression
        baseline_speedup = self._get_baseline_speedup(test_name)
        if baseline_speedup is not None and speedup < baseline_speedup * 0.9:
            raise PerformanceRegression(
                f"{test_name} speedup dropped to {speedup:.2f}x "
                f"(baseline: {baseline_speedup:.2f}x)"
            )
    
    def _get_baseline_speedup(self, test_name: str) -> Optional[float]:
        """
        Get baseline speedup for a test.
        
        Returns the average speedup from the last 5 successful runs.
        """
        if test_name not in self.results or len(self.results[test_name]) == 0:
            return None
        
        # Get last 5 results
        recent_results = self.results[test_name][-5:]
        speedups = [r["speedup"] for r in recent_results]
        
        return sum(speedups) / len(speedups)
    
    def _get_git_commit(self) -> str:
        """Get current git commit hash."""
        try:
            result = subprocess.run(
                ["git", "rev-parse", "--short", "HEAD"],
                capture_output=True,
                text=True,
                check=True
            )
            return result.stdout.strip()
        except (subprocess.CalledProcessError, FileNotFoundError):
            return "unknown"
    
    def get_report(self) -> str:
        """
        Generate a performance report.
        
        Returns:
            str: Formatted performance report
        """
        if not self.results:
            return "No benchmark results available."
        
        report = []
        report.append("=== GPU Performance Report ===")
        report.append(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}")
        report.append("")
        
        # Table header
        report.append(f"{'Test':<30} {'CPU Time':<12} {'GPU Time':<12} {'Speedup':<10} {'Status':<8}")
        report.append("─" * 80)
        
        # Calculate statistics for each test
        for test_name, results in self.results.items():
            if not results:
                continue
            
            # Get latest result
            latest = results[-1]
            cpu_time = latest["cpu_time"]
            gpu_time = latest["gpu_time"]
            speedup = latest["speedup"]
            
            # Determine status (assume 2.0x minimum for Phase 1)
            status = "✅ PASS" if speedup >= 2.0 else "❌ FAIL"
            
            report.append(
                f"{test_name:<30} "
                f"{cpu_time*1000:>9.1f}ms "
                f"{gpu_time*1000:>9.1f}ms "
                f"{speedup:>8.2f}x "
                f"{status:<8}"
            )
        
        report.append("")
        
        # Calculate overall average speedup
        all_speedups = []
        for results in self.results.values():
            if results:
                all_speedups.append(results[-1]["speedup"])
        
        if all_speedups:
            avg_speedup = sum(all_speedups) / len(all_speedups)
            target_speedup = 2.5  # Phase 1 goal
            overall_status = "✅ PASS" if avg_speedup >= 2.0 else "❌ FAIL"
            report.append(
                f"Overall: {avg_speedup:.2f}x average speedup "
                f"(Target: {target_speedup:.1f}x) {overall_status}"
            )
        
        return "\n".join(report)
    
    def clear(self):
        """Clear all benchmark results."""
        self.results = {}
        if self.results_file.exists():
            self.results_file.unlink()


def generate_performance_report(output_file: str = "benchmark_report.md"):
    """
    Generate a markdown performance report.
    
    Args:
        output_file: Path to output markdown file
    """
    tracker = BenchmarkTracker()
    report = tracker.get_report()
    
    with open(output_file, 'w') as f:
        f.write("# GPU Performance Benchmark Report\n\n")
        f.write("```\n")
        f.write(report)
        f.write("\n```\n")
        f.write("\n## Speedup Trends\n\n")
        
        # Add trend information for each test
        for test_name, results in tracker.results.items():
            if len(results) < 2:
                continue
            
            f.write(f"### {test_name}\n\n")
            f.write("| Commit | CPU Time | GPU Time | Speedup |\n")
            f.write("|--------|----------|----------|---------|\n")
            
            # Show last 10 results
            for result in results[-10:]:
                commit = result["commit"][:7]
                cpu_time = result["cpu_time"] * 1000  # Convert to ms
                gpu_time = result["gpu_time"] * 1000
                speedup = result["speedup"]
                f.write(f"| {commit} | {cpu_time:.1f}ms | {gpu_time:.1f}ms | {speedup:.2f}x |\n")
            
            f.write("\n")
    
    print(f"Performance report generated: {output_file}")


if __name__ == "__main__":
    # Generate report when run directly
    generate_performance_report()
