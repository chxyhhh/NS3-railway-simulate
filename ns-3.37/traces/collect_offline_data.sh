#!/bin/bash
# 第四章：批量采集离线训练数据
# 基于切换时刻 + 不同提前量，用 --triggerSource=time 触发
#
# 配置方法：
#   1. 跑一次 baseline 确认切换时刻，填入 HO_TIMES
#   2. 修改 ADVANCE_LIST 设置要扫描的提前量
#   3. bash collect_offline_data.sh

NS3_DIR="$HOME/repos/ns-3-allinone/ns-3.37"
OUTPUT_BASE="$NS3_DIR/offline_data"
PROGRAM="scratch/tcp_high_speed_train-ori"

# ═══════════════════════════════════════════════
# 【配置区】— 基站/速度改了就改 HO_TIMES
# ═══════════════════════════════════════════════

HO_TIMES=(7.656 19.656 31.656 43.656 55.656)
ADVANCE_LIST=(0.2 0.3 0.5 0.8 1.0 1.5 2.0)

# ═══════════════════════════════════════════════

mkdir -p "$OUTPUT_BASE"
RUN_ID=0

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

    echo "[RUN $RUN_ID] $tag"
    cd "$NS3_DIR"
    ./ns3 run $PROGRAM -- "$@" > "$run_dir/stdout.log" 2>&1

    if [ -d "traces" ]; then
        cp traces/*.csv "$run_dir/" 2>/dev/null
        cp traces/*.txt "$run_dir/" 2>/dev/null
        cp traces/*.pcap "$run_dir/" 2>/dev/null
    fi

    if [ -f "$run_dir/timeseries.csv" ]; then
        local rows=$(wc -l < "$run_dir/timeseries.csv")
        echo "  → OK ($rows rows)"
    else
        echo "  → FAILED"
        tail -3 "$run_dir/stdout.log"
    fi
}

echo "============================================"
echo "  第四章 离线数据采集"
echo "  切换时刻: ${HO_TIMES[*]}"
echo "  提前量:   ${ADVANCE_LIST[*]}"
echo "  注意: 主体实验不加随机丢包"
echo "============================================"

# ────────────────────────────────────────────────
# A. Baseline × 多 seeds（无优化，无随机丢包）
# ────────────────────────────────────────────────
for seed in 1 2 3 4 5; do
    run_sim "baseline_s${seed}" \
        --expCase=baseline --rngRun=$seed
done

# ────────────────────────────────────────────────
# B. Rwnd-only 自适应 × 不同提前量 × 多 seeds
# ────────────────────────────────────────────────
for adv in "${ADVANCE_LIST[@]}"; do
    trigger_times=$(calc_trigger_times $adv)
    for seed in 1 2 3; do
        run_sim "rwnd_adv${adv}_s${seed}" \
            --expCase=rwnd-only --paramMode=adaptive \
            --triggerSource=time --triggerTimesSec=$trigger_times \
            --rngRun=$seed
    done
done

# ────────────────────────────────────────────────
# C. Joint 自适应 × 不同提前量 × 多 seeds
# ────────────────────────────────────────────────
for adv in 0.3 0.5 0.8 1.0 1.5; do
    trigger_times=$(calc_trigger_times $adv)
    for seed in 1 2 3; do
        run_sim "joint_adv${adv}_s${seed}" \
            --expCase=joint --paramMode=adaptive \
            --triggerSource=time --triggerTimesSec=$trigger_times \
            --rngRun=$seed
    done
done

# ────────────────────────────────────────────────
# D. 固定参数 × 不同提前量（对比自适应）
# ────────────────────────────────────────────────
for alpha in 0.3 0.6; do
    for adv in 0.3 0.5 1.0; do
        trigger_times=$(calc_trigger_times $adv)
        run_sim "rwnd_fixed_a${alpha}_adv${adv}" \
            --expCase=rwnd-only --paramMode=fixed --rwndAlphaFloor=$alpha \
            --triggerSource=time --triggerTimesSec=$trigger_times \
            --rngRun=1
    done
done

# ────────────────────────────────────────────────
# E. 不同丢包率环境（泛化性对比，单独一组）
# ────────────────────────────────────────────────
for plr in 0.01 0.02 0.05; do
    run_sim "baseline_plr${plr}" \
        --expCase=baseline --rngRun=2 --pktLossRate=$plr

    trigger_times=$(calc_trigger_times 0.5)
    run_sim "joint_plr${plr}" \
        --expCase=joint --paramMode=adaptive \
        --triggerSource=time --triggerTimesSec=$trigger_times \
        --rngRun=2 --pktLossRate=$plr
done

echo ""
echo "============================================"
echo "  采集完成！共 $RUN_ID 组仿真"
echo "  数据目录: $OUTPUT_BASE"
echo "============================================"
echo ""
total_rows=0; ok=0; fail=0
for d in "$OUTPUT_BASE"/run_*/; do
    if [ -f "$d/timeseries.csv" ]; then
        rows=$(wc -l < "$d/timeseries.csv")
        total_rows=$((total_rows + rows))
        ok=$((ok + 1))
    else
        fail=$((fail + 1))
        echo "  MISSING: $(basename $d)"
    fi
done
echo "  成功: $ok 组 | 失败: $fail 组"
echo "  总数据行: $total_rows"
