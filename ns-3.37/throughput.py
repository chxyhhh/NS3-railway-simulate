import matplotlib.pyplot as plt

# 读取吞吐量数据
with open('rxThroughput_data.txt', 'r') as file:
    rxThroughput_data = [float(line.strip()) for line in file.readlines()]

# 读取丢包率数据
with open('packet_loss_data.txt', 'r') as file:
    packet_loss_data = [float(line.strip()) for line in file.readlines()]


# 从文件读取数据
time_data = []
rtt_data = []
file_path = "rttTr.txt"
with open(file_path, "r") as file:
    for line in file:
        # 跳过空行
        if line.strip():
            # 按空格拆分每行，获取时间和 RTT
            time, rtt = line.split()
            time_data.append(float(time))  # 时间（秒）
            rtt_data.append(float(rtt) * 1000)  # RTT 转换为毫秒

# # 从文件读取数据
# timee_data = []
# cwnd_data = []
# file_path = "cwndTr.txt"
# with open(file_path, "r") as file:
#     for line in file:
#         # 跳过空行
#         if line.strip():
#             # 按空格拆分每行，获取时间和 RTT
#             timee, cwnd = line.split()
#             timee_data.append(float(timee))  # 时间（秒）
#             cwnd_data.append(int(cwnd))

# 计算平均吞吐量
average_rxThroughput = sum(rxThroughput_data) / len(rxThroughput_data) if rxThroughput_data else 0

# 输出平均吞吐量
print(f"Average txThroughput: {average_rxThroughput:.2f} Mbps")

# 绘制吞吐量曲线
plt.figure(figsize=(12, 6))
# plt.subplot(3, 1, 1)
plt.plot(rxThroughput_data, label="rxThroughput (Mbps)")
plt.xlabel('Time (seconds)')
plt.ylabel('rxThroughput (Mbps)')
plt.title('Network rxThroughput Over Time')
#plt.yticks([5, 10, 15])  # 设置y轴刻度为5, 10, 15
plt.grid(True)
plt.legend()
plt.show()


# 创建图形
# plt.subplot(3, 1, 2)
# 绘制 RTT vs. Time 图
plt.plot(time_data, rtt_data, marker='o', linestyle='-', color='b', label="RTT (ms)")
# 设置横坐标间隔为 10 秒
plt.xticks(range(int(min(time_data)), int(max(time_data)) + 1, 10))
plt.title("RTT vs Time", fontsize=16)
plt.xlabel("Time (s)", fontsize=14)
plt.ylabel("RTT (ms)", fontsize=14)
plt.grid(True)
plt.legend()
plt.show()

# 绘制丢包率曲线
# plt.subplot(3, 1, 3)
plt.plot(packet_loss_data, label="Packet Loss Rate (%)", color='red')
plt.xlabel('Time (seconds)')
plt.ylabel('Packet Loss Rate (%)')
plt.title('Packet Loss Rate Over Time')
plt.grid(True)
plt.legend()

plt.tight_layout()
plt.show()

# # 创建图形
# plt.plot
# # 绘制 RTT vs. Time 图
# plt.plot(timee_data, cwnd_data, marker='o', linestyle='-', color='b', label="cwnd")
# # 设置横坐标间隔为 10 秒
# plt.xticks(range(int(min(timee_data)), int(max(timee_data)) + 1, 10))
# plt.yticks(range(0, int(max(cwnd_data)+50000), 50000))
# plt.title("cwnd vs Time", fontsize=16)
# plt.xlabel("Time (s)", fontsize=14)
# plt.ylabel("cwnd", fontsize=14)
# plt.grid(True)
# plt.legend()
# plt.show()
