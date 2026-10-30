Fit30：通用只读取值融合与慢绘制归因
版本0.2.114-keyui7-gpuf5-fit30；系统版本0.2.114-k7-f30。
Title ID沿用05004A324D450000。

实际优化：在既有read-leaf路径内识别两种完整、只读的小方法：
aload_0/getfield/return，以及实例short[]按索引读取并&65535返回。
省去有效调用的scratch Frame/容器初始化与逐条解释；原callee成本仍为3/7
个操作。不新增JIT IC/机器码、不改方法元数据、不按游戏名/JAR作白名单。
字段未解析、空引用、数组越界、类型/常量池/预算不满足时仍走原路径，
由原Java调用处理异常。P61逐操作诊断保留原执行方式。

原版刺客信条固定精灵回放：三组配对调用线程CPU减少4.4349%、1.3043%、
1.6129%，中位数1.6129%。像素、原操作量、绘图、GC、叶方法调用数一致。
收益较小，只证明该受控宿主负载；不代表Switch整局FPS或三款游戏60帧。

诊断修正：旧worker等待统计无法覆盖主上下文paint等待。本版在DIAGEXEC
增加paint/main-paint观察到的cap/poll/native等待；PAINTSLOW绑定同一次最长
paint的CPU和等待。复用已有CPU时钟读取，不新增逐操作时钟查询。
CPU不可用为-1；观察等待可重叠，不能覆盖全部OS调度和未插桩I/O。
字段为附加信息、非独占时间拆分，且不进入存档。

没有修改游戏等待、时钟、调度/音频策略、Java库、游戏、设置或存档格式。
新日志中的蜘蛛侠316ms间隔有原游戏主动300ms sleep证据，保持其语义。
JIT常量准备复用经过测量，成本太小，不进入本版。

源码/测试：src/read_leaf.inc，runtime_profile.h，runtime_diagnostics.inc，
paint_hotspots241.h及repaint_current的归因接线。新增验证在tests/runtime_fit30，
原版回放、全部组合测试、目标构建和NSP校验详见FIT30-验证报告.txt及证据包。
三个智能体均为gpt-6.1-sol/high，分别负责取值优化、停顿调查/诊断、验证构建。
没有实体Switch Fit30测量；源代码与内容校验通过不等于实机性能验收。
