#!/bin/bash
# 第四章：批量采集离线训练数据
# 用法：在 ns-3.37 目录下运行
#   bash collect_offline_data.sh

NS3_DIR="$HOME/repos/ns-3-allinone/ns-3.37"
OUTPUT_BASE="$NS3_DIR/offline_data"
PROGRAM="scratch/tcp_high_speed_train-ori"

mkdir -p "$OUTPUT_BASE"

RUN_ID=0

run_sim() {
    local tag="$1"
    shift
    # 剩余的全部是参数
    RUN_ID=$((RUN_ID + 1))
    local run_dir="$OUTPUT_BASE/run_$(printf '%03d' $RUN_ID)_${tag}"
    mkdir -p "$run_dir"

    echo "[RUN $RUN_ID] $tag"
    echo "  args: $@"
    cd "$NS3_DIR"

    # 关键：程序名和参数之间用 -- 分隔，且不能放在同一个引号里
    ./ns3 run $PROGRAM -- "$@" > "$run_dir/stdout.log" 2>&1

    if [ -d "traces" ]; then
        cp traces/*.csv "$run_dir/" 2>/dev/null
        cp traces/*.txt "$run_dir/" 2>/dev/null
    fi

    # 快速验证：检查数据是否生成
    if [ -f "$run_dir/timeseries.csv" ]; then
        local rows=$(wc -l < "$run_dir/timeseries.csv")
        echo "  → OK ($rows rows)"
    else
        echo "  → FAILED (no timeseries.csv)"
        cat "$run_dir/stdout.log" | tail -5
    fi
}

echo "============================================"
echo "  第四章 离线数据采集"
echo "============================================"

# ── 1. Baseline × 5 seeds ──
for seed in 2 3 4 5 6; do
    run_sim "baseline_s${seed}" \
        --expCase=baseline --enableRandom=true --rngRun=$seed --pktLossRate=0.01
done

# ── 2. Baseline × 不同丢包率 ──
for plr in 0.005 0.02 0.03 0.05; do
    run_sim "baseline_plr${plr}" \
        --expCase=baseline --enableRandom=true --rngRun=10 --pktLossRate=$plr
done

# ── 3. Rwnd-only 自适应 × 5 seeds ──
for seed in 2 3 4 5 6; do
    run_sim "rwnd_adaptive_s${seed}" \
        --expCase=rwnd-only --paramMode=adaptive \
        --enableRandom=true --rngRun=$seed --pktLossRate=0.01
done

# ── 4. Joint 自适应 × 5 seeds ──
for seed in 2 3 4 5 6; do
    run_sim "joint_adaptive_s${seed}" \
        --expCase=joint --paramMode=adaptive \
        --enableRandom=true --rngRun=$seed --pktLossRate=0.01
done

# ── 5. 固定参数扫描（alpha） ──
for alpha in 0.2 0.3 0.4 0.6 0.8; do
    run_sim "rwnd_fixed_a${alpha}" \
        --expCase=rwnd-only --paramMode=fixed --rwndAlphaFloor=$alpha \
        --enableRandom=true --rngRun=3 --pktLossRate=0.01
done

# ── 6. Joint 固定参数组合 ──
for alpha in 0.3 0.6; do
    for m in 2 3 4; do
        run_sim "joint_fixed_a${alpha}_m${m}" \
            --expCase=joint --paramMode=fixed \
            --rwndAlphaFloor=$alpha --ackSplitCount=$m \
            --enableRandom=true --rngRun=3 --pktLossRate=0.01
    done
done

echo ""
echo "============================================"
echo "  采集完成！共 $RUN_ID 组仿真"
echo "  数据目录: $OUTPUT_BASE"
echo "============================================"
echo ""
echo "验证数据："
for d in "$OUTPUT_BASE"/run_*/; do
    if [ -f "$d/timeseries.csv" ]; then
        rows=$(wc -l < "$d/timeseries.csv")
        echo "  $(basename $d): $rows rows"
    else
        echo "  $(basename $d): MISSING"
    fi
done
