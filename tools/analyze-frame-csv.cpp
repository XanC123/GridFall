#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

struct Metrics {
    explicit Metrics(std::string label) : name(std::move(label)) {}
    std::string name;
    std::vector<double> values;
    void print() {
        if (values.empty()) return;
        std::sort(values.begin(), values.end());
        const double sum = std::accumulate(values.begin(), values.end(), 0.0);
        const auto percentile = [&](double p) {
            const std::size_t index = static_cast<std::size_t>(p * static_cast<double>(values.size() - 1));
            return values[index];
        };
        const std::size_t overBudget = static_cast<std::size_t>(std::count_if(values.begin(), values.end(),
            [](double v) { return v > 10.42; }));
        std::cout << std::fixed << std::setprecision(3)
                  << name << ": mean=" << sum / static_cast<double>(values.size())
                  << " ms p95=" << percentile(0.95) << " ms p99=" << percentile(0.99)
                  << " ms max=" << values.back() << " ms spikes>10.42ms=" << overBudget
                  << '/' << values.size() << '\n';
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/gridfall-frame-samples.csv\n";
        return 2;
    }
    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "Cannot open: " << argv[1] << '\n';
        return 2;
    }
    std::array<Metrics, 5> metrics{{
        Metrics{"Frame period"}, Metrics{"Game update CPU"}, Metrics{"Render submission CPU"},
        Metrics{"EndDrawing/present wait"}, Metrics{"Input-poll to render-submit proxy"}
    }};
    std::string line;
    std::getline(input, line); // CSV header
    while (std::getline(input, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        std::size_t sample{};
        std::array<double, 5> values{};
        if (!(row >> sample >> values[0] >> values[1] >> values[2] >> values[3] >> values[4])) continue;
        for (std::size_t i = 0; i < metrics.size(); ++i) metrics[i].values.push_back(values[i]);
    }
    if (metrics[0].values.empty()) {
        std::cerr << "No numeric samples found in CSV.\n";
        return 1;
    }
    std::cout << "GRIDFALL GRAPHICAL FRAME SAMPLE SUMMARY\n"
              << "Samples: " << metrics[0].values.size() << "\n"
              << "144 Hz budget: 6.944 ms/frame; spike threshold: 10.42 ms/frame\n"
              << "F11 exports the in-game rolling window (up to 4096 samples).\n"
              << "Input metric is poll-to-CPU-render-submit, not true input-to-photon latency.\n";
    for (Metrics& metric : metrics) metric.print();
    return 0;
}
