/**
 * @file make_change.cpp
 * @brief C++ implementation of the Make Change practical work.
 *
 * In this program, we implement and compare the methods requested during the
 * course: greedy selection, recursive enumeration of all valid solutions,
 * dynamic programming, and recursive branch-and-bound with cuts.
 *
 * Authors: Ahmed Al-Muharaq and Owais Khan
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using Clock = std::chrono::high_resolution_clock;
using Microseconds = std::chrono::duration<double, std::micro>;

struct Solution {
    std::vector<int> counts;
    bool feasible = false;
    int numberOfCoins = 0;
};

struct SearchStatistics {
    long long exploredStates = 0;
    long long prunedStates = 0;
    long long validSolutions = 0;
    double elapsedMicroseconds = 0.0;
};

struct SearchResult {
    Solution best;
    SearchStatistics statistics;
    std::vector<Solution> improvingSolutions;
};

/**
 * We validate the denominations, remove duplicates and sort them from the
 * largest value to the smallest value. We use integer cents everywhere to
 * avoid floating-point errors.
 */
std::vector<int> normalizeCoins(std::vector<int> coins) {
    if (coins.empty()) {
        throw std::invalid_argument("The coin list cannot be empty.");
    }

    for (int coin : coins) {
        if (coin <= 0) {
            throw std::invalid_argument("Every coin value must be positive.");
        }
    }

    std::sort(coins.begin(), coins.end(), std::greater<>());
    coins.erase(std::unique(coins.begin(), coins.end()), coins.end());
    return coins;
}

int countCoins(const std::vector<int>& counts) {
    return std::accumulate(counts.begin(), counts.end(), 0);
}

std::string formatMoney(int cents) {
    std::ostringstream output;
    output << "EUR " << cents / 100 << '.' << std::setw(2) << std::setfill('0')
           << std::abs(cents % 100);
    return output.str();
}

void printSolution(const std::vector<int>& coins, const Solution& solution) {
    if (!solution.feasible) {
        std::cout << "No exact solution\n";
        return;
    }

    bool first = true;
    for (std::size_t i = 0; i < coins.size(); ++i) {
        if (solution.counts[i] == 0) {
            continue;
        }
        if (!first) {
            std::cout << " + ";
        }
        std::cout << solution.counts[i] << " x " << formatMoney(coins[i]);
        first = false;
    }
    std::cout << "  (" << solution.numberOfCoins << " coins)\n";
}

/**
 * We apply the greedy rule to the list exactly as it is provided. This lets us
 * demonstrate why the order of the denominations matters.
 */
Solution greedyAssumingCurrentOrder(const std::vector<int>& coins, int amount,
                                    long long* operationCounter = nullptr) {
    if (amount < 0) {
        throw std::invalid_argument("The amount cannot be negative.");
    }

    Solution result;
    result.counts.assign(coins.size(), 0);
    int remaining = amount;

    for (std::size_t i = 0; i < coins.size(); ++i) {
        result.counts[i] = remaining / coins[i];
        remaining %= coins[i];
        if (operationCounter != nullptr) {
            ++(*operationCounter);
        }
    }

    result.feasible = (remaining == 0);
    result.numberOfCoins = countCoins(result.counts);
    return result;
}

/**
 * When the input is unordered, we first normalize it and then apply the same
 * greedy rule. The greedy scan is linear, while sorting adds O(n log n).
 */
Solution greedyWithNormalization(std::vector<int> coins, int amount,
                                 std::vector<int>* normalized = nullptr) {
    coins = normalizeCoins(std::move(coins));
    if (normalized != nullptr) {
        *normalized = coins;
    }
    return greedyAssumingCurrentOrder(coins, amount);
}

/**
 * We recursively generate every possible count for every denomination. A leaf
 * is a valid solution only when its remaining amount is zero.
 */
void enumerateRecursive(const std::vector<int>& coins, int index, int remaining,
                        std::vector<int>& partial, SearchStatistics& statistics,
                        std::vector<Solution>* storedSolutions = nullptr,
                        std::size_t storageLimit = std::numeric_limits<std::size_t>::max()) {
    ++statistics.exploredStates;

    if (index == static_cast<int>(coins.size())) {
        if (remaining == 0) {
            ++statistics.validSolutions;
            if (storedSolutions != nullptr && storedSolutions->size() < storageLimit) {
                storedSolutions->push_back({partial, true, countCoins(partial)});
            }
        }
        return;
    }

    const int coin = coins[index];
    const int maximumCount = remaining / coin;

    // We try large counts first, as requested in the course.
    for (int count = maximumCount; count >= 0; --count) {
        partial[index] = count;
        enumerateRecursive(coins, index + 1, remaining - count * coin,
                           partial, statistics, storedSolutions, storageLimit);
    }
    partial[index] = 0;
}

SearchStatistics enumerateAllSolutions(const std::vector<int>& inputCoins, int amount,
                                       std::vector<Solution>* storedSolutions = nullptr,
                                       std::size_t storageLimit = 20) {
    const std::vector<int> coins = normalizeCoins(inputCoins);
    std::vector<int> partial(coins.size(), 0);
    SearchStatistics statistics;

    const auto start = Clock::now();
    enumerateRecursive(coins, 0, amount, partial, statistics,
                       storedSolutions, storageLimit);
    statistics.elapsedMicroseconds = Microseconds(Clock::now() - start).count();
    return statistics;
}

/**
 * We compute an initial upper bound with the greedy method. If greedy fails,
 * the bound remains infinite and the exact search finds the first incumbent.
 */
SearchResult bestSolutionWithCuts(const std::vector<int>& inputCoins, int amount) {
    const std::vector<int> coins = normalizeCoins(inputCoins);
    SearchResult result;

    const int commonDivisor = std::accumulate(
        coins.begin(), coins.end(), 0,
        [](int current, int coin) { return std::gcd(current, coin); });

    if (amount % commonDivisor != 0) {
        result.statistics.exploredStates = 1;
        result.statistics.prunedStates = 1;
        return result;
    }

    Solution greedy = greedyAssumingCurrentOrder(coins, amount);
    int bestCount = std::numeric_limits<int>::max();
    if (greedy.feasible) {
        result.best = greedy;
        bestCount = greedy.numberOfCoins;
        result.improvingSolutions.push_back(greedy);
    }

    std::vector<int> partial(coins.size(), 0);

    const auto start = Clock::now();

    const auto search = [&](auto&& self, int index, int remaining, int used) -> void {
        ++result.statistics.exploredStates;

        if (remaining == 0) {
            ++result.statistics.validSolutions;
            if (used < bestCount) {
                bestCount = used;
                result.best = {partial, true, used};
                result.improvingSolutions.push_back(result.best);
            }
            return;
        }

        if (index == static_cast<int>(coins.size()) || used >= bestCount) {
            ++result.statistics.prunedStates;
            return;
        }

        // Since all future coins are no larger than coins[index], this is an
        // admissible lower bound on the number of additional coins.
        const int optimisticAdditional =
            static_cast<int>(std::ceil(static_cast<double>(remaining) / coins[index]));

        if (bestCount != std::numeric_limits<int>::max() &&
            used + optimisticAdditional >= bestCount) {
            ++result.statistics.prunedStates;
            return;
        }

        const int maximumCount = remaining / coins[index];
        for (int count = maximumCount; count >= 0; --count) {
            if (bestCount != std::numeric_limits<int>::max() &&
                used + count >= bestCount) {
                ++result.statistics.prunedStates;
                continue;
            }

            partial[index] = count;
            self(self, index + 1, remaining - count * coins[index], used + count);
        }
        partial[index] = 0;
    };

    search(search, 0, amount, 0);
    result.statistics.elapsedMicroseconds = Microseconds(Clock::now() - start).count();
    return result;
}

/**
 * We also provide a bottom-up dynamic-programming verification. It computes
 * one minimum-coin solution in O(number of denominations * amount) time.
 */
Solution dynamicProgrammingMinimum(const std::vector<int>& inputCoins, int amount) {
    const std::vector<int> coins = normalizeCoins(inputCoins);
    const int infinity = amount + 1;
    std::vector<int> minimum(amount + 1, infinity);
    std::vector<int> chosenCoin(amount + 1, -1);
    minimum[0] = 0;

    for (int current = 1; current <= amount; ++current) {
        for (std::size_t i = 0; i < coins.size(); ++i) {
            if (coins[i] <= current && minimum[current - coins[i]] + 1 < minimum[current]) {
                minimum[current] = minimum[current - coins[i]] + 1;
                chosenCoin[current] = static_cast<int>(i);
            }
        }
    }

    if (minimum[amount] == infinity) {
        return {};
    }

    Solution result;
    result.feasible = true;
    result.numberOfCoins = minimum[amount];
    result.counts.assign(coins.size(), 0);
    for (int current = amount; current > 0;) {
        const int index = chosenCoin[current];
        ++result.counts[index];
        current -= coins[index];
    }
    return result;
}

void writeGreedyBenchmark() {
    std::ofstream file("greedy_complexity.csv");
    file << "denominations,ordered_operations,ordered_time_us,unordered_normalized_time_us\n";

    const std::vector<int> sizes = {10, 50, 100, 200, 500, 1000, 2000, 5000};
    constexpr int repetitions = 300;

    for (int size : sizes) {
        std::vector<int> ordered(size);
        std::iota(ordered.rbegin(), ordered.rend(), 1);
        const int amount = size * 50 + 17;

        long long operations = 0;
        const auto orderedStart = Clock::now();
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            long long currentOperations = 0;
            greedyAssumingCurrentOrder(ordered, amount, &currentOperations);
            operations = currentOperations;
        }
        const double orderedTime =
            Microseconds(Clock::now() - orderedStart).count() / repetitions;

        std::vector<int> unordered = ordered;
        std::mt19937 generator(42);
        std::shuffle(unordered.begin(), unordered.end(), generator);

        const auto unorderedStart = Clock::now();
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            greedyWithNormalization(unordered, amount);
        }
        const double unorderedTime =
            Microseconds(Clock::now() - unorderedStart).count() / repetitions;

        file << size << ',' << operations << ',' << orderedTime << ',' << unorderedTime << '\n';
    }
}

void writeRecursiveBenchmark() {
    std::ofstream file("recursive_complexity.csv");
    file << "coin_types,amount,all_states,valid_solutions,cut_states,pruned_states\n";

    for (int numberOfTypes = 2; numberOfTypes <= 14; ++numberOfTypes) {
        std::vector<int> coins(numberOfTypes);
        std::iota(coins.rbegin(), coins.rend(), 1);
        const int amount = 3 * numberOfTypes;

        const SearchStatistics exhaustive = enumerateAllSolutions(coins, amount, nullptr, 0);
        const SearchResult cut = bestSolutionWithCuts(coins, amount);

        file << numberOfTypes << ',' << amount << ','
             << exhaustive.exploredStates << ',' << exhaustive.validSolutions << ','
             << cut.statistics.exploredStates << ',' << cut.statistics.prunedStates << '\n';
    }
}

void runOfficialExample() {
    const std::vector<int> ordered = {500, 200, 100, 50, 20, 10, 5};
    const std::vector<int> unordered = {20, 500, 50, 100, 5, 200, 10};
    constexpr int amount = 1235;

    std::cout << "\nOfficial example: " << formatMoney(amount) << "\n";

    std::cout << "Ordered greedy solution: ";
    printSolution(ordered, greedyAssumingCurrentOrder(ordered, amount));

    std::cout << "Unordered list used directly: ";
    printSolution(unordered, greedyAssumingCurrentOrder(unordered, amount));

    std::vector<int> normalized;
    const Solution normalizedGreedy = greedyWithNormalization(unordered, amount, &normalized);
    std::cout << "Unordered list after sorting: ";
    printSolution(normalized, normalizedGreedy);

    std::vector<Solution> samples;
    const SearchStatistics enumeration = enumerateAllSolutions(ordered, amount, &samples, 5);
    std::cout << "Recursive enumeration found " << enumeration.validSolutions
              << " valid solutions after exploring " << enumeration.exploredStates
              << " states.\n";

    const SearchResult cut = bestSolutionWithCuts(ordered, amount);
    std::cout << "Improving incumbents kept by the exact search:\n";
    for (const Solution& improvement : cut.improvingSolutions) {
        std::cout << "  ";
        printSolution(ordered, improvement);
    }
    std::cout << "Best solution with cuts: ";
    printSolution(ordered, cut.best);
    std::cout << "States explored: " << cut.statistics.exploredStates
              << ", states pruned: " << cut.statistics.prunedStates << "\n";

    std::cout << "Dynamic-programming verification: ";
    printSolution(ordered, dynamicProgrammingMinimum(ordered, amount));
}

void runCounterexamples() {
    {
        const std::vector<int> coins = {400, 300, 100};
        constexpr int amount = 600;
        std::cout << "\nCounterexample where greedy is not optimal\n";
        std::cout << "Greedy: ";
        printSolution(coins, greedyAssumingCurrentOrder(coins, amount));
        std::cout << "Exact with cuts: ";
        const SearchResult exact = bestSolutionWithCuts(coins, amount);
        printSolution(coins, exact.best);
        std::cout << "Improving sequence: ";
        for (const Solution& improvement : exact.improvingSolutions) {
            printSolution(coins, improvement);
            std::cout << "                    ";
        }
    }

    {
        const std::vector<int> coins = {500, 200, 150};
        constexpr int amount = 950;
        std::cout << "\nCounterexample where greedy fails\n";
        std::cout << "Greedy: ";
        printSolution(coins, greedyAssumingCurrentOrder(coins, amount));
        std::cout << "Exact with cuts: ";
        printSolution(coins, bestSolutionWithCuts(coins, amount).best);
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--benchmark") {
            writeGreedyBenchmark();
            writeRecursiveBenchmark();
            std::cout << "Benchmark CSV files generated.\n";
            return 0;
        }

        runOfficialExample();
        runCounterexamples();
        std::cout << "\nRun with --benchmark to generate the CSV data used for the curves.\n";
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
