#pragma once
#include <functional>
#include <string>

struct BatchResult {
    int converted;
    int failed;
    int skipped;
};

// Run a batch conversion from source_dir to output_dir.
// log_fn is called with each line of output (no trailing newline).
// Returns result counts on completion.
BatchResult run_batch(const std::string& source_dir,
                      const std::string& output_dir,
                      bool dry_run,
                      bool force,
                      const std::function<void(const std::string&)>& log_fn);
