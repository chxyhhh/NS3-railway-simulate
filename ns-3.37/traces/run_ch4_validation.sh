#!/bin/bash
# ============================================================
# 第四章验证实验：DDQN 策略 vs Baseline vs 第三章方案
# ============================================================
#
# 前置条件：
#   1. 将 policy_offline_table.json 传到 Linux 的 NS-3 目录
#   2. 编译: cd ~/repos/ns-3-allinone/ns-3.37 && ./ns3 build
#
# 用法：
#   bash run_ch4_validation.sh
#
# 输出在 ch4_validation/ 目录下

NS3_DIR="$HOME/repos/ns-3-allinone/ns-3.37"
OUTPUT_BASE="$NS3_DIR/ch4_validation"
PROGRAM="scratch/tcp_high_speed_train"
POLICY_TABLE="$NS3_DIR/policy_offline_table.json"

# 切换时刻（与离线数据采集一致）
HO_TIMES=(7.656 19.656 31.656 43.656 55.656)
# 第三章方案的最优提前量（从离线实验选出）
BEST_ADV=0.5

mkdir -p "$OUTPUT_BASE"
RUN_ID=0

# 计算提前触发时刻
calc_trigger_times() {
    local advance=$1
    local result=""
    for ho_t in "${HO_TIMES[@]}"; do
        local trig=$(awk "BEGIN {t=$ho_t - $advance; if(t<0.5) t=0.5; printf \"%.3f\", t}")
        if [ -z "$result" ]; then result="$trig"; else result="$result,$trig"; fi
    done
    echo "$result"
}

run_sim() {
    local tag="$1"
    shift
    RUN_ID=$((RUN_ID + 1))
    local run_dir="$OUTPUT_BASE/run_$(printf '%03d' $RUN_ID)_${tag}"
    mkdir -p "$run_dir"

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "[RUN $RUN_ID] $tag"
    echo "  args: $@"
    cd "$NS3_DIR"
    ./ns3 run "$PROGRAM" -- "$@" > "$run_dir/stdout.log" 2>&1

    if [ -d "traces" ]; then
        cp traces/*.csv "$run_dir/" 2>/dev/null
        cp traces/*.txt "$run_dir/" 2>/dev/null
        cp traces/*.pcap "$run_dir/" 2>/dev/null
    fi

    if [ -f "$run_dir/timeseries.csv" ]; then
        local rows=$(wc -l < "$run_dir/timeseries.csv")
        echo "  → OK ($rows rows)"
    else
        echo "  → FAILED (check $run_dir/stdout.log)"
        tail -5 "$run_dir/stdout.log"
    fi
}

echo "============================================"
echo "  第四章 验证实验"
echo "  策略表: $POLICY_TABLE"
echo "============================================"

# 检查策略表文件是否存在
if [ ! -f "$POLICY_TABLE" ]; then
    echo "[ERROR] 策略表文件不存在: $POLICY_TABLE"
    echo "请先将 policy_offline_table.json 拷贝到 NS-3 目录"
    exit 1
fi

TRIGGER_TIMES=$(calc_trigger_times $BEST_ADV)

# ────────────────────────────────────────────
# G1: Baseline（无优化）× 5 seeds
# ────────────────────────────────────────────
echo ""
echo ">>> G1: Baseline"
for seed in 1 2 3 4 5; do
    run_sim "G1_baseline_s${seed}" \
        --expCase=baseline --rngRun=$seed
done

# ────────────────────────────────────────────
# G2: 第三章 Joint 固定参数 × 5 seeds
# ────────────────────────────────────────────
echo ""
echo ">>> G2: Rule-Fixed (joint, fixed params)"
for seed in 1 2 3 4 5; do
    run_sim "G2_rule_fixed_s${seed}" \
        --expCase=joint --paramMode=fixed \
        --triggerSource=time --triggerTimesSec=$TRIGGER_TIMES \
        --rngRun=$seed
done

# ────────────────────────────────────────────
# G3: 第三章 Joint 自适应参数 × 5 seeds
# ────────────────────────────────────────────
echo ""
echo ">>> G3: Rule-Adaptive (joint, adaptive params)"
for seed in 1 2 3 4 5; do
    run_sim "G3_rule_adaptive_s${seed}" \
        --expCase=joint --paramMode=adaptive \
        --triggerSource=time --triggerTimesSec=$TRIGGER_TIMES \
        --rngRun=$seed
done

# ────────────────────────────────────────────
# G4: LSTM+DDQN 策略查表 × 5 seeds
# ────────────────────────────────────────────
echo ""
echo ">>> G4: LSTM+DDQN"
for seed in 1 2 3 4 5; do
    run_sim "G4_lstm_ddqn_s${seed}" \
        --expCase=ddqn --policyTablePath=$POLICY_TABLE \
        --rngRun=$seed
done

# ────────────────────────────────────────────
# 汇总
# ────────────────────────────────────────────
echo ""
echo "============================================"
echo "  验证实验完成！共 $RUN_ID 组仿真"
echo "  数据目录: $OUTPUT_BASE"
echo "============================================"
echo ""

# 简单统计
ok=0; fail=0
for d in "$OUTPUT_BASE"/run_*/; do
    if [ -f "$d/timeseries.csv" ]; then
        ok=$((ok + 1))
    else
        fail=$((fail + 1))
        echo "  MISSING: $(basename $d)"
    fi
done
echo "  成功: $ok 组 | 失败: $fail 组"
echo ""
echo "下一步："
echo "  1. 将 $OUTPUT_BASE 拷贝回 Windows"
echo "  2. 运行 Python 出图脚本"
