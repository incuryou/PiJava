# ZXX scheduling skeleton A/B test

Run from any directory:
```sh
python3 benchmarks/zxx/run.py --baseline /path/to/0.2.31 --build-only
python3 benchmarks/zxx/run.py --baseline /path/to/0.2.31 --skip-build --repeats 3
```
Stop compilers/other benchmarks first. Each observation is 3 seconds after 3
seconds warmup. This fixture models the verified 50ms owner gate and a 100ms
auxiliary branch. `patch_fixture.py` modifies only the fixture branch encoding,
never the user's game. Isolated copies are instrumented with a 5ms PAINT sleep
and 2ms presentation sleep. This is imposed **wall latency, not CPU raster work**.
The test is a scheduling/control-flow demonstration, not a measurement of ZXX
gameplay FPS or Switch performance. Clock-scale acceleration is preserved.
All original files, isolation diffs, inputs and observations are hashed/saved
in build/zxx-bench. Production source is not modified by this script.
