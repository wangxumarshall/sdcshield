"""tools/excite/tests 会话级测试环境(Task 6 nodepool)。

PIPELINE_NODEPOOL=0 是 task_loop_nodepool 自带的安全阀(docstring 明言
"冒烟/测试用"): pick_node 只返回本机、零 SSH 探测。tl.dispatch_agent /
tp.dispatch_review 自 Task 6 起 node=None 会真调 np.pick_node——既有
dispatch 测试(test_task_loop / test_ci_triage / test_task_loop_pipeline)
均未打 np 桩,不打阀则每次派发真探活三板(ConnectTimeout=8×3,且在能连
板网的机器上择优结果取决于当天板上内存——单测非密闭、断言分叉)。
本阀把既有用例钉死在纯本机旧行为; 远程分支用例自打 np.* 桩、不经真
pick_node,不受影响; np 阀语义用例(test_pick_node_env_valves 等)
自设 env 覆盖本默认。"""
import os

os.environ["PIPELINE_NODEPOOL"] = "0"
