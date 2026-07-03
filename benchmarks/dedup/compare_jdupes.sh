#!/bin/bash
set -e

DEDUP_BIN="$1"
if [ -z "$DEDUP_BIN" ] || [ ! -x "$DEDUP_BIN" ]; then
    echo "Usage: $0 <path_to_dedup_executable>"
    exit 1
fi

WORK_DIR="/tmp/dedup_sys_benchmark"
JDUPES_DIR="$WORK_DIR/jdupes_src"
JDUPES_BIN="$JDUPES_DIR/jdupes"
DATA_DIR="$WORK_DIR/dataset"

mkdir -p "$WORK_DIR"

if [ ! -x "$JDUPES_BIN" ]; then
    echo "Fetching and building jdupes (v1.20.0)..."
    rm -rf "$JDUPES_DIR"
    git clone --depth 1 -b v1.20.0 https://codeberg.org/jbruchon/jdupes.git "$JDUPES_DIR" >/dev/null 2>&1
    make -C "$JDUPES_DIR" >/dev/null 2>&1
fi

if [ ! -d "$DATA_DIR" ]; then
    echo "Generating test datasets (approx 2.5 GB)..."
    echo "This might take a moment, but is optimized via a random data pool."
    mkdir -p "$DATA_DIR/mixed"
    
    # 优化点 1：生成一个巨大的基础随机池 (约 500MB)，避免每次都调用缓慢的 /dev/urandom
    POOL_FILE="$WORK_DIR/random_pool.bin"
    dd if=/dev/urandom of="$POOL_FILE" bs=1M count=500 status=none
    
    # helper: 从随机池中快速切片出指定大小的文件，通过 skip 保证内容不同
    gen_file() { 
        local file_path="$1"
        local size_kb="$2"
        # 随机跳过 0~1000 个 block，确保每次切片内容不同
        local skip_blocks=$((RANDOM % 1000))
        dd if="$POOL_FILE" of="$file_path" bs=1024 count="$size_kb" skip="$skip_blocks" status=none
    }
    
    # 1. 大量内容不同的小文件 (500个，每个1MB -> 500MB)
    echo " -> Generating small unique files..."
    mkdir -p "$DATA_DIR/mixed/small_unique"
    for i in {1..500}; do gen_file "$DATA_DIR/mixed/small_unique/u_$i.bin" 1024; done

    # 2. 许多重复的小文件 (20组，每组2MB基础文件 + 9个副本 -> 400MB)
    echo " -> Generating small duplicates..."
    mkdir -p "$DATA_DIR/mixed/small_dups"
    for i in {1..20}; do
        gen_file "$DATA_DIR/mixed/small_dups/base_$i.bin" 2048
        for j in {1..9}; do cp "$DATA_DIR/mixed/small_dups/base_$i.bin" "$DATA_DIR/mixed/small_dups/dup_${i}_${j}.bin"; done
    done

    # 3. 大文件重复 (3个基础文件，每个200MB + 2个副本 -> 1.8GB)
    echo " -> Generating large duplicates..."
    mkdir -p "$DATA_DIR/mixed/large_dups"
    for i in {1..3}; do
        gen_file "$DATA_DIR/mixed/large_dups/base_${i}.bin" 204800
        for j in {1..2}; do cp "$DATA_DIR/mixed/large_dups/base_${i}.bin" "$DATA_DIR/mixed/large_dups/dup_${i}_${j}.bin"; done
    done
    
    # 删除临时内存池
    rm -f "$POOL_FILE"
fi

echo ""
echo "| 工具 | 数据集 | wall time(s) | user time(s) | sys time(s) | max RSS(KB) |"
echo "| --- | --- | ---: | ---: | ---: | ---: |"

run_bench() {
    local name="$1"
    shift
    local cmd=("$@")
    
    # 优化点 2：预热运行 (Warmup)
    # 因为我们不使用 sudo 清理系统缓存(drop_caches)，先跑的工具会将文件读入 RAM，导致后跑的工具直接读取热缓存而占据极大优势。
    # 解决方法：在正式计时前，让当前工具先完整的跑一遍，确保文件系统缓存处于一致的“热”状态，纯测试多线程与哈希 CPU 吞吐量。
    "${cmd[@]}" > /dev/null 2>&1
    
    if command -v /usr/bin/time >/dev/null 2>&1; then
        # 优化点 3：严谨的计时
        /usr/bin/time -f "%e\t%U\t%S\t%M" "${cmd[@]}" > /dev/null 2> "$WORK_DIR/time.out"
        local wall=$(awk '{print $1}' "$WORK_DIR/time.out")
        local user=$(awk '{print $2}' "$WORK_DIR/time.out")
        local sys=$(awk '{print $3}' "$WORK_DIR/time.out")
        local rss=$(awk '{print $4}' "$WORK_DIR/time.out")
        printf "| %-18s | mixed | %12s | %12s | %11s | %11s |\n" "$name" "$wall" "$user" "$sys" "$rss"
    else
        echo -n "| $name | mixed | "
        time "${cmd[@]}" > /dev/null
    fi
}

THREADS="${DEDUP_THREADS:-8}"

run_bench "jdupes" "$JDUPES_BIN" -r "$DATA_DIR/mixed"
run_bench "dedup ($THREADS threads)" "$DEDUP_BIN" "$DATA_DIR/mixed" --threads $THREADS

echo ""
