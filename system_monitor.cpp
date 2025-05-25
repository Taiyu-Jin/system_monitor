#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QWidget>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <sys/statvfs.h>
#include <cstdio>

class SystemMonitor : public QMainWindow {
    Q_OBJECT
public:
    SystemMonitor(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("System Monitor");
        resize(400, 300);

        // 设置主窗口布局
        QWidget *centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        QVBoxLayout *layout = new QVBoxLayout(centralWidget);

        // 创建标签用于显示系统信息
        cpuLabel = new QLabel("CPU Usage: Calculating...", this);
        memoryLabel = new QLabel("Memory Usage: Calculating...", this);
        diskLabel = new QLabel("Disk Usage: Calculating...", this);
        gpuUsageLabel = new QLabel("GPU Usage: Calculating...", this);
        gpuTempLabel = new QLabel("GPU Temperature: Calculating...", this);

        layout->addWidget(cpuLabel);
        layout->addWidget(memoryLabel);
        layout->addWidget(diskLabel);
        layout->addWidget(gpuUsageLabel);
        layout->addWidget(gpuTempLabel);

        // 设置定时器，每秒更新一次
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &SystemMonitor::updateSystemInfo);
        timer->start(1000);

        updateSystemInfo(); // 初始更新
    }

private slots:
    void updateSystemInfo() {
        updateCPUUsage();
        updateMemoryUsage();
        updateDiskUsage();
        updateGPUInfo();
    }

private:
    QLabel *cpuLabel;
    QLabel *memoryLabel;
    QLabel *diskLabel;
    QLabel *gpuUsageLabel;
    QLabel *gpuTempLabel;

    unsigned long long prevIdle = 0, prevTotal = 0;

    void updateCPUUsage() {
        std::ifstream statFile("/proc/stat");
        std::string line;
        std::getline(statFile, line); // 读取第一行（cpu 总计）

        std::istringstream iss(line);
        std::string cpu;
        unsigned long long user, nice, system, idle, iowait, irq, softirq;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;

        unsigned long long total = user + nice + system + idle + iowait + irq + softirq;
        unsigned long long idleTime = idle + iowait;

        if (prevTotal > 0) {
            double totalDiff = total - prevTotal;
            double idleDiff = idleTime - prevIdle;
            double cpuUsage = 100.0 * (totalDiff - idleDiff) / totalDiff;
            cpuLabel->setText(QString("CPU Usage: %1%").arg(cpuUsage, 0, 'f', 2));
        }

        prevIdle = idleTime;
        prevTotal = total;
        statFile.close();
    }

    void updateMemoryUsage() {
        std::ifstream memFile("/proc/meminfo");
        std::string line;
        unsigned long totalMem = 0, freeMem = 0, availableMem = 0;

        while (std::getline(memFile, line)) {
            std::istringstream iss(line);
            std::string key;
            unsigned long value;
            iss >> key >> value;
            if (key == "MemTotal:") totalMem = value;
            else if (key == "MemFree:") freeMem = value;
            else if (key == "MemAvailable:") availableMem = value;
        }

        double usedMem = totalMem - availableMem;
        double memoryUsage = 100.0 * usedMem / totalMem;
        memoryLabel->setText(QString("Memory Usage: %1% (%2 MB / %3 MB)")
                             .arg(memoryUsage, 0, 'f', 2)
                             .arg(usedMem / 1024.0, 0, 'f', 2)
                             .arg(totalMem / 1024.0, 0, 'f', 2));
        memFile.close();
    }

    void updateDiskUsage() {
        struct statvfs stat;
        if (statvfs("/", &stat) == 0) {
            unsigned long long total = stat.f_blocks * stat.f_frsize;
            unsigned long long free = stat.f_bavail * stat.f_frsize;
            unsigned long long used = total - free;
            double diskUsage = 100.0 * used / total;
            diskLabel->setText(QString("Disk Usage: %1% (%2 GB / %3 GB)")
                              .arg(diskUsage, 0, 'f', 2)
                              .arg(used / (1024.0 * 1024 * 1024), 0, 'f', 2)
                              .arg(total / (1024.0 * 1024 * 1024), 0, 'f', 2));
        }
    }

    void updateGPUInfo() {
        FILE *pipe = popen("nvidia-smi --query-gpu=utilization.gpu,temperature.gpu --format=csv -i 0", "r");
        if (!pipe) {
            gpuUsageLabel->setText("GPU Usage: N/A (nvidia-smi not found)");
            gpuTempLabel->setText("GPU Temperature: N/A");
            return;
        }

        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);

        // 解析nvidia-smi输出
        std::istringstream iss(result);
        std::string line;
        std::getline(iss, line); // 跳过标题行
        std::getline(iss, line); // 读取数据行

        std::istringstream dataStream(line);
        std::string gpuUsage, gpuTemp;
        std::getline(dataStream, gpuUsage, ',');
        std::getline(dataStream, gpuTemp, ',');

        // 清理数据（去除百分比符号和单位）
        gpuUsage.erase(std::remove_if(gpuUsage.begin(), gpuUsage.end(), ::isspace), gpuUsage.end());
        gpuUsage.erase(gpuUsage.find('%'), 1);
        gpuTemp.erase(std::remove_if(gpuTemp.begin(), gpuTemp.end(), ::isspace), gpuTemp.end());

        if (!gpuUsage.empty() && !gpuTemp.empty()) {
            gpuUsageLabel->setText(QString("GPU Usage: %1%").arg(gpuUsage.c_str()));
            gpuTempLabel->setText(QString("GPU Temperature: %1°C").arg(gpuTemp.c_str()));
        } else {
            gpuUsageLabel->setText("GPU Usage: N/A");
            gpuTempLabel->setText("GPU Temperature: N/A");
        }
    }
};

#include "system_monitor.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    SystemMonitor monitor;
    monitor.show();
    return app.exec();
}