# Advanced Draft Strategy Tool

**A strategic drafting companion for Brawl Stars.**

**Important:** Because recommendations are produced by an MCTS guided by a heuristic evaluator derived from historical statistics, the tool provides *statistically strong picks on average* — useful guidance for robust choices — but it does not guarantee optimal picks for every individual draft or rare tactical situations.

This application analyses a large dataset of historical game results to provide statistical insights and intelligent recommendations during character selection. It prioritises high‑rank games, offers both fast heuristic suggestions and deeper Monte Carlo Tree Search (MCTS) analysis, and exposes a GUI draft simulator to test picks and bans interactively.

---

## Features

* **Comprehensive data analysis** — processes a `.jsonl` file containing thousands of game results to build a robust statistical model.
* **Rank‑weighted statistics** — gives more weight to higher‑rank games to reflect competitive meta trends.
* **Efficient caching** — computed statistics are saved to a binary cache file for near‑instant subsequent startups.
* **Interactive draft simulator** — full GUI to simulate a draft with picks, bans and undo/redo controls.
* **Dual suggestion modes**:

  * *Heuristic Suggestions* — instant recommendations using a weighted formula (win rate, synergy, counters, pick rate).
  * *MCTS Deep Analysis* — multi‑threaded Monte Carlo Tree Search for forward‑looking evaluation.
* **Ban recommendations** — suggests impactful bans for the selected map/mode.
* **Full draft control** — undo picks, unban characters, reset draft state.
* **Configurable parameters** — tweak heuristic weights and MCTS settings via `draft_config.ini`.

---

## How it works

The application is driven by a preprocessed binary cache file (`stats.pack`) that contains aggregated, rank‑weighted statistics derived from historical games. The GUI and both suggestion modes (heuristic and MCTS) read from this cache to power simulations and recommendations. If `stats.pack` is missing on first run, the app will generate it from the raw dataset; subsequent launches load the cache for fast startup.

---

## Prerequisites

* C++17 compatible compiler (GCC, Clang, MSVC)
* CMake 3.16+
* Qt 6 (Core, Gui, Widgets, Concurrent modules)
* (Optional) `ninja` or `make` for building

---

## Building from source

```bash
# Clone the repository
git clone <repository-url>
cd <repository-directory>

# Create a build directory and run CMake
mkdir build
cd build
# If Qt is not on the default path, give CMake a hint:
# cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/<arch>
cmake ..

# Build the project
cmake --build .
# or, on UNIX with Makefiles:
# make -j$(nproc)
```

The final executable will be placed in the build output directory (e.g. `build/` or `build/bin/` depending on your generator).

---

## Usage

1. **Place the data file**

   The application requires a data file named **`high_level_ranked_games.jsonl`** in the same directory as the executable. The app will refuse to start without it.

2. **First run**

   On first launch the app will process the entire `.jsonl` file. This may take several minutes depending on file size and system performance. The app will create:

   * `stats.pack` (binary cache of processed statistics)
   * `draft_log.log` (processing and runtime log)

3. **Subsequent runs**

   After the cache is created the app will load `stats.pack` and the main window will appear almost instantly.

4. **Simulator controls**

   * Select **Mode** and **Map** from the dropdowns to begin a draft.
   * Use the **Available Brawlers** list to pick a character.
   * Click **Pick T1**, **Pick T2**, or **Ban** to perform actions. Double‑click performs the likely default action (pick or ban depending on turn).
   * **Undo Pick**, **Unban**, and **Reset Draft** are available to revert changes.
   * **Suggest Pick (Fast)** provides an instant heuristic recommendation.
   * **Suggest Pick (Deep)** runs MCTS (UI locks while running). Use **Stop MCTS** to cancel early.

---

## Configuration (`draft_config.ini`)

`draft_config.ini` is created on first run and can be edited to tweak behavior.

Example:

```ini
[Settings]
MctsTimeLimit = 10          # seconds (default time limit for MCTS)
SmoothingK = 5              # Laplace smoothing parameter to avoid extreme win rates
RankWeightExponent = 1.5    # exponent controlling rank weighting
PickRateThreshold = 0.01    # minimum pick rate to consider

[Weights]
WinRate = 1.0
Synergy = 0.8
Counter = 0.9
PickRate = 0.3

[MCTS]
Threads = 4
ExplorationConstant = 1.414
MaxDepth = 32

[Paths]
DataFile = high_level_ranked_games.jsonl
CacheFile = stats.pack

```

**Notes:**

* `MctsTimeLimit` controls how long the deep analysis runs by default.
* `SmoothingK` prevents tiny sample sizes from producing 0% or 100% win rates.
* Heuristic weights (`WinRate`, `Synergy`, `Counter`, `PickRate`) control the scoring used by the fast suggestion mode.

---

## Data format

Each line of `high_level_ranked_games.jsonl` should be one JSON object representing a single game. The exact schema depends on your data export but must include at minimum:

* teams and their character IDs/names
* result/winner
* player ranks (for rank weighting)
* map/mode identifiers

If you need a schema example, open an issue in the repository or check the `data/schema/` directory (if present).

---

## Troubleshooting

* **App refuses to start**: ensure `high_level_ranked_games.jsonl` is present in the executable directory.
* **First run takes a long time**: processing is expected; subsequent runs load `stats.pack`.
* **MCTS runs too long or uses all CPU**: lower `MctsTimeLimit` or `Threads` in `draft_config.ini`.
* **Results look noisy**: increase `SmoothingK` or raise `PickRateThreshold` to ignore very rare picks.

---

## Contributing

Contributions and bug reports are welcome. Please open issues or pull requests against the repository. If you're adding data or changing the schema, include an updated schema file and a small sample dataset for testing.

---

## License & Contact

Include your preferred license here. For bugs or questions contact: `me@oliver-w.com`.

---

*Happy drafting!*
