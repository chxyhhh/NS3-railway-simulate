#!/bin/bash
# 第四章：批量采集离线训练数据
# 用法：在 ns-3.37 目录下运行
#   bash collect_offline_data.sh
#
# 输出：offline_data/run_XXX/ 每个包含 timeseries.csv, optim_trace.csv, handover_events.csv

NS3_DIR="$HOME/repos/ns-3-allinone/ns-3.37"
OUTPUT_BASE="$NS3_DIR/offline_data"
PROGRAM="scratch/tcp_high_speed_train-ori"

mkdir -p "$OUTPUT_BASE"

RUN_ID=0

run_sim() {
    local args="$1"
    local tag="$2"
    RUN_ID=$((RUN_ID + 1))
    local run_dir="$OUTPUT_BASE/run_$(printf '%03d' $RUN_ID)_${tag}"
    mkdir -p "$run_dir"

    echo "[RUN $RUN_ID] $tag"
    cd "$NS3_DIR"
    ./ns3 run "$PROGRAM -- $args" > "$run_dir/stdout.log" 2>&1

    if [ -d "traces" ]; then
        cp traces/*.csv "$run_dir/" 2>/dev/null
        cp traces/*.txt "$run_dir/" 2>/dev/null
    fi
    echo "  → $run_dir (done)"
}

echo "============================================"
echo "  第四章 离线数据采集（enableRandom）"
echo "============================================"

# ── 1. Baseline × 5 seeds ──
for seed in 1 2 3 4 5; do
    run_sim "--expCase=baseline --enableRandom=true --rngRun=$seed --pktLossRate=0.01" "baseline_s${seed}"
done

# ── 2. Baseline × 不同丢包率 ──
for plr in 0.005 0.01 0.02 0.03; do
    run_sim "--expCase=baseline --enableRandom=true --rngRun=10 --pktLossRate=$plr" "baseline_plr${plr}"
done

# ── 3. Rwnd-only 自适应 × 5 seeds ──
for seed in 1 2 3 4 5; do
    run_sim "--expCase=rwnd-only --paramMode=adaptive --enableRandom=true --rngRun=$seed --pktLossRate=0.01" "rwnd_adaptive_s${seed}"
done

# ── 4. Joint 自适应 × 5 seeds ──
for seed in 1 2 3 4 5; do
    run_sim "--expCase=joint --paramMode=adaptive --enableRandom=true --rngRun=$seed --pktLossRate=0.01" "joint_adaptive_s${seed}"
done

# ── 5. 固定参数扫描（alpha） ──
for alpha in 0.2 0.3 0.4 0.6 0.8; do
    run_sim "--expCase=rwnd-only --paramMode=fixed --rwndAlphaFloor=$alpha --enableRandom=true --rngRun=1 --pktLossRate=0.01" "rwnd_fixed_a${alpha}"
done

# ── 6. Joint 固定参数组合 ──
for alpha in 0.3 0.6; do
    for m in 2 3 4; do
        run_sim "--expCase=joint --paramMode=fixed --rwndAlphaFloor=$alpha --ackSplitCount=$m --enableRandom=true --rngRun=1 --pktLossRate=0.01" "joint_fixed_a${alpha}_m${m}"
    done
done

echo ""
echo "============================================"
echo "  采集完成！共 $RUN_ID 组仿真"
echo "  数据目录: $OUTPUT_BASE"
echo "============================================"
