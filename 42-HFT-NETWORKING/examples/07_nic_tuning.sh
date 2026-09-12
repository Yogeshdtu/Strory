#!/usr/bin/env bash
# 07_nic_tuning.sh
# ============================================================
# NIC tuning checklist -- ring buffers, interrupt coalescing, offloads,
# RSS/IRQ affinity, sysctls. LINUX-ONLY, root/NET_ADMIN chahiye (is repo
# ka box Windows/MinGW hai -> yeh script yahan chalegi nahi; real NIC
# waale Linux box pe chalao). `folder`/`checkall` ise compile nahi karte
# (.sh, C++ nahi) -- 09-nic-tuning.md, 10-irq-affinity-tuning.md.
#
# ⚠️ Yeh commands NIC settings PERMANENTLY (until reboot/next script)
# badalte hain -- production interface pe pehle STAGING mein test karo.
# ============================================================

set -euo pipefail
IFACE="${1:?usage: sudo ./07_nic_tuning.sh <interface>   (e.g. eth0)}"

echo "================================================================"
echo " 1. Current state -- BASELINE se pehle, likh lo (rollback ke liye)"
echo "================================================================"
ethtool -g "$IFACE"   || echo "  (ring params query fail -- driver support check karo)"
ethtool -c "$IFACE"   || echo "  (coalescing query fail)"
ethtool -k "$IFACE"   || echo "  (offload features query fail)"

echo
echo "================================================================"
echo " 2. RX/TX ring buffers -- BADA karo (bursty market-data feed ko"
echo "    kernel-level drop se bachata jab tak app thodi der bhi peeche ho)"
echo "================================================================"
# `ethtool -g` se max values dekho, phir unke KAREEB set karo (bahut bada
# bhi latency badhata -- queueing delay, ek trade-off, 35/05's principle).
ethtool -G "$IFACE" rx 4096 tx 4096 \
    || echo "  (driver max se bada maanga ho sakta -- 'ethtool -g' ke max dekho)"

echo
echo "================================================================"
echo " 3. Interrupt coalescing -- MIN/OFF karo (latency ke liye, throughput"
echo "    ke against trade-off -- zyaada interrupts = zyaada CPU, kam batching)"
echo "================================================================"
ethtool -C "$IFACE" rx-usecs 0 rx-frames 1 tx-usecs 0 tx-frames 1 \
    || echo "  (coalescing tuning driver-specific -- kuch flags unsupported ho sakte)"

echo
echo "================================================================"
echo " 4. Offloads -- GRO/LRO OFF (HFT ko HAR packet turant, ALAG chahiye,"
echo "    NA ki kernel-merged 'bade' packets jo latency add karte)"
echo "================================================================"
ethtool -K "$IFACE" gro off lro off \
    || echo "  (GRO/LRO off fail -- kuch drivers LRO support hi nahi karte)"
# TSO/GSO/checksumming offloads TX-side throughput ke liye theek hain
# (chhoti market-data-style packets pe unka asar kam hota); disable karna
# hai to explicitly evaluate karo, blanket-off mat karo.

echo
echo "================================================================"
echo " 5. RSS queues + pinned IRQs -- har RX queue ka interrupt ek ALAG,"
echo "    DEDICATED (housekeeping se alag) core pe pin karo"
echo "================================================================"
ethtool -l "$IFACE" || echo "  (channel/queue-count query fail)"
echo "  IRQ list is interface ke liye:"
grep "$IFACE" /proc/interrupts || echo "  ($IFACE /proc/interrupts mein nahi mila)"
echo "  Har IRQ ko pin karo (example -- APNE core-numbers se replace karo):"
echo '    echo 4 > /proc/irq/<IRQ_NUM>/smp_affinity_list      # core 4 pe pin'
echo "  RPS (Receive Packet Steering) hot queues ke liye OFF rakho -- yeh"
echo "  software-level re-steering hai, extra latency add karta:"
echo "    echo 0 > /sys/class/net/$IFACE/queues/rx-0/rps_cpus"

echo
echo "================================================================"
echo " 6. Pause frames OFF (upstream congestion se poori NIC stall NAHI"
echo "    honi chahiye ek burst ki wajah se)"
echo "================================================================"
ethtool -A "$IFACE" rx off tx off \
    || echo "  (pause-frame control driver-specific)"

echo
echo "================================================================"
echo " 7. qdisc -- pfifo_fast ya mq (multi-queue), fq/fq_codel jaisa"
echo "    latency-adding shaping HOT path interface pe mat rakho"
echo "================================================================"
tc qdisc show dev "$IFACE" || true
echo "  Agar zaroorat ho: tc qdisc replace dev $IFACE root pfifo_fast"

echo
echo "================================================================"
echo " 8. sysctls -- system-wide socket buffer aur backlog limits"
echo "================================================================"
cat <<'EOF'
  sudo sysctl -w net.core.rmem_max=134217728
  sudo sysctl -w net.core.wmem_max=134217728
  sudo sysctl -w net.core.netdev_max_backlog=250000
  sudo sysctl -w net.core.busy_poll=50        # microseconds, system-wide default (07)
  sudo sysctl -w net.core.busy_read=50
EOF

echo
echo "================================================================"
echo " 9. Verify BY DROP COUNTERS -- tuning 'lag raha' feel se nahi,"
echo "    counters se PROVE karo"
echo "================================================================"
echo "  ethtool -S $IFACE | grep -iE 'drop|discard|miss|error'"
echo "  cat /proc/net/softnet_stat   # 2nd column = dropped, non-zero = tune more"

echo
echo "Done. Rollback: driver reload (rmmod/modprobe) ya reboot se defaults"
echo "wapas aa jaate. Har change ke baad drop-counters (step 9) SE verify karo."
