# Hierarchical QoS Traffic Shaper & Bandwidth Manager (HQTS)
# Hierarchical QoS Traffic Shaper & Bandwidth Manager (HQTS)

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/company/hqts)
[![License](https://img.shields.io/badge/license-Commercial-blue)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0--dev-orange)](CHANGELOG.md)

A high-performance, enterprise-grade traffic management system for layer 2/3 switches and routers that provides sophisticated bandwidth allocation, traffic shaping, congestion management, and priority-based scheduling with support for complex hierarchical policies.

## 🚀 Key Features

- **100 Gbps Line Rate Performance** - High-throughput packet processing with <50μs latency
- **8-Level Hierarchical Policies** - Complex bandwidth allocation trees with parent-child relationships
- **Advanced Scheduling Algorithms** - WFQ, WRR, DRR, HFSC, and Strict Priority scheduling
- **Real-time SLA Monitoring** - Comprehensive service level agreement enforcement
- **Multi-tenant Support** - Isolated bandwidth management for multiple tenants
- **Machine Learning Optimization** - Predictive bandwidth allocation and congestion management
- **Comprehensive APIs** - REST APIs and SNMP integration for management
- **High Availability** - 99.99% uptime with hot-standby failover

## 📋 Requirements

### System Requirements
- **CPU**: Intel Xeon or AMD EPYC with AVX2 support
- **Memory**: Minimum 16GB RAM (32GB recommended for production)
- **Network**: 10Gbps+ network interfaces
- **OS**: Linux kernel 5.4+ (Ubuntu 20.04+, RHEL 8+, or CentOS 8+)

### Dependencies
- **C++17** compatible compiler (GCC 9+, Clang 10+)
- **Boost Libraries** 1.71+ (multi_index, asio, system, thread)
- **DPDK** 21.11+ (for high-performance packet processing)
- **libpcap** 1.9+ (for packet capture)
- **OpenSSL** 1.1.1+ (for secure management)
- **protobuf** 3.12+ (for configuration serialization)

## 🏗️ Project Structure

```
hqts/
├── README.md                   # This file
├── LICENSE                     # Commercial license
├── CHANGELOG.md               # Version history
├── CMakeLists.txt             # Main CMake configuration
├── vcpkg.json                 # Package manager configuration
├── .clang-format              # Code formatting rules
├── .github/                   # GitHub workflows
│   ├── workflows/
│   │   ├── ci.yml            # Continuous integration
│   │   └── release.yml       # Release automation
│   └── ISSUE_TEMPLATE/       # Issue templates
├── docs/                      # Documentation
│   ├── api/                  # API documentation
│   ├── design/               # System design documents
│   ├── user-guide/           # User documentation
│   └── developer-guide/      # Development documentation
├── include/                   # Public header files
│   └── hqts/
│       ├── core/             # Core system headers
│       ├── policy/           # Policy management
│       ├── scheduler/        # Scheduling algorithms
│       ├── monitor/          # Monitoring and statistics
│       └── api/              # Public API headers
├── src/                       # Source implementation
│   ├── core/                 # Core system implementation
│   │   ├── token_bucket.cpp
│   │   ├── flow_context.cpp
│   │   ├── memory_pool.cpp
│   │   └── timer_manager.cpp
│   ├── policy/               # Policy management
│   │   ├── policy_tree.cpp
│   │   ├── policy_engine.cpp
│   │   ├── hierarchy_manager.cpp
│   │   └── sla_monitor.cpp
│   ├── scheduler/            # Scheduling algorithms
│   │   ├── wfq_scheduler.cpp
│   │   ├── drr_scheduler.cpp
│   │   ├── hfsc_scheduler.cpp
│   │   └── priority_scheduler.cpp
│   ├── dataplane/            # Data plane processing
│   │   ├── packet_processor.cpp
│   │   ├── flow_classifier.cpp
│   │   ├── queue_manager.cpp
│   │   └── rate_controller.cpp
│   ├── monitor/              # Monitoring and statistics
│   │   ├── statistics_collector.cpp
│   │   ├── performance_monitor.cpp
│   │   └── anomaly_detector.cpp
│   ├── management/           # Management plane
│   │   ├── rest_api.cpp
│   │   ├── config_manager.cpp
│   │   ├── snmp_agent.cpp
│   │   └── web_dashboard.cpp
│   └── main.cpp              # Application entry point
├── tests/                     # Test suite
│   ├── unit/                 # Unit tests
│   │   ├── core/
│   │   ├── policy/
│   │   ├── scheduler/
│   │   └── dataplane/
│   ├── integration/          # Integration tests
│   ├── performance/          # Performance benchmarks
│   ├── stress/               # Stress tests
│   └── utils/                # Test utilities
├── tools/                     # Development and deployment tools
│   ├── policy-validator/     # Policy validation tool
│   ├── traffic-generator/    # Traffic generation for testing
│   ├── performance-analyzer/ # Performance analysis tools
│   └── config-converter/     # Configuration conversion utilities
├── scripts/                   # Build and deployment scripts
│   ├── build.sh              # Build script
│   ├── install.sh            # Installation script
│   ├── run-tests.sh          # Test execution
│   └── deploy.sh             # Deployment script
├── config/                    # Configuration files
│   ├── examples/             # Example configurations
│   ├── schemas/              # Configuration schemas
│   └── templates/            # Configuration templates
├── docker/                    # Docker configuration
│   ├── Dockerfile            # Main container
│   ├── docker-compose.yml    # Multi-container setup
│   └── dev.dockerfile        # Development container
└── third_party/              # Third-party dependencies
    ├── dpdk/                 # DPDK submodule
    └── json/                 # JSON library
```

## 🛠️ Building from Source

### Prerequisites Installation

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y build-essential cmake git
sudo apt install -y libboost-all-dev libpcap-dev libssl-dev
sudo apt install -y protobuf-compiler libprotobuf-dev
sudo apt install -y libnuma-dev pkg-config
```

**RHEL/CentOS:**
```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y cmake3 git boost-devel libpcap-devel openssl-devel
sudo yum install -y protobuf-devel protobuf-compiler numactl-devel
```

### Building HQTS

1. **Clone the repository:**
   ```bash
   git clone --recursive https://github.com/company/hqts.git
   cd hqts
   ```

2. **Install DPDK (if not using system package):**
   ```bash
   cd third_party/dpdk
   meson build
   cd build
   ninja
   sudo ninja install
   cd ../../..
   ```

3. **Configure and build:**
   ```bash
   mkdir build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   make -j$(nproc)
   ```

4. **Run tests:**
   ```bash
   gtest
   ```

### Build Options

- `CMAKE_BUILD_TYPE`: `Debug`, `Release`, `RelWithDebInfo` (default: `Release`)
- `HQTS_ENABLE_DPDK`: Enable DPDK acceleration (default: `ON`)
- `HQTS_ENABLE_TESTS`: Build test suite (default: `ON`)
- `HQTS_ENABLE_BENCHMARKS`: Build performance benchmarks (default: `OFF`)
- `HQTS_ENABLE_TOOLS`: Build development tools (default: `ON`)

**Example with custom options:**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DHQTS_ENABLE_BENCHMARKS=ON ..
```

## 🚀 Quick Start

### 1. Basic Installation

```bash
# Install HQTS
sudo make install

# Create configuration directory
sudo mkdir -p /etc/hqts
sudo cp config/examples/basic.json /etc/hqts/hqts.json

# Start the service
sudo systemctl enable hqts
sudo systemctl start hqts
```

### 2. Simple Configuration Example

```json
{
  "version": "1.0",
  "interfaces": [
    {
      "name": "eth0",
      "bandwidth": "10Gbps",
      "policies": [
        {
          "name": "voice_traffic",
          "priority": 1,
          "guaranteed_rate": "1Gbps",
          "max_rate": "2Gbps",
          "classification": {
            "dscp": [46, 34]
          }
        },
        {
          "name": "data_traffic",
          "priority": 2,
          "guaranteed_rate": "5Gbps",
          "max_rate": "8Gbps",
          "classification": {
            "protocols": ["tcp", "udp"],
            "ports": [80, 443, 22]
          }
        }
      ]
    }
  ]
}
```

### 3. Verify Installation

```bash
# Check service status
sudo systemctl status hqts

# View basic statistics
curl http://localhost:8080/api/v1/stats

# Test policy configuration
hqts-cli policy validate /etc/hqts/hqts.json
```

## 📊 Performance Characteristics

| Metric | Value | Test Conditions |
|--------|-------|----------------|
| **Maximum Throughput** | 100 Gbps | Line rate processing |
| **Processing Latency** | < 50 μs | Added delay worst-case |
| **Concurrent Flows** | 1M+ | Active shaped flows |
| **Policy Scale** | 100K+ | Active policies |
| **Memory per Flow** | < 512 bytes | Per-flow context |
| **CPU Utilization** | < 60% | At line rate |
| **Packet Rate** | 148.8 Mpps | Minimum-sized packets |

## 🔧 Configuration

### Policy Hierarchy Example

```json
{
  "root_policy": {
    "interface": "eth0",
    "bandwidth": "10Gbps",
    "children": [
      {
        "name": "business_critical",
        "guaranteed_rate": "6Gbps",
        "max_rate": "8Gbps",
        "children": [
          {
            "name": "voice",
            "guaranteed_rate": "1Gbps",
            "scheduler": "strict_priority",
            "priority": 1
          },
          {
            "name": "video",
            "guaranteed_rate": "2Gbps",
            "scheduler": "wfq",
            "weight": 30
          }
        ]
      },
      {
        "name": "best_effort",
        "guaranteed_rate": "2Gbps",
        "max_rate": "4Gbps",
        "scheduler": "drr",
        "quantum": 1500
      }
    ]
  }
}
```

### Advanced Features

- **SLA Monitoring**: Real-time bandwidth guarantee enforcement
- **Multi-tenancy**: Isolated bandwidth pools per tenant
- **Adaptive Policies**: ML-driven bandwidth adjustment
- **Congestion Management**: Predictive congestion avoidance
- **Statistics Export**: SNMP, REST API, streaming telemetry

## 📈 Monitoring & Management

### REST API Endpoints

```bash
# System status
GET /api/v1/status

# Policy management
GET /api/v1/policies
POST /api/v1/policies
PUT /api/v1/policies/{id}
DELETE /api/v1/policies/{id}

# Statistics
GET /api/v1/stats/interface/{name}
GET /api/v1/stats/flow/{id}
GET /api/v1/stats/policy/{id}

# Real-time monitoring
GET /api/v1/monitor/bandwidth
GET /api/v1/monitor/flows
GET /api/v1/monitor/sla-violations
```

### Web Dashboard

Access the web dashboard at `http://localhost:8080/dashboard` for:
- Real-time bandwidth visualization
- Policy configuration management
- SLA compliance monitoring
- Performance analytics
- System health monitoring

## 🧪 Testing

### Unit Tests
```bash
cd build
Google test
```

### Performance Benchmarks
```bash
cd build
./tests/performance/throughput_benchmark
./tests/performance/latency_benchmark
./tests/performance/scalability_benchmark
```

### Stress Testing
```bash
cd build
./tests/stress/policy_stress_test
./tests/stress/flow_stress_test --flows 1000000
```

## 🔍 Development

### Code Style
- Follow Google C++ Style Guide
- Use clang-format for automatic formatting
- Comprehensive unit test coverage (>90%)
- Performance benchmarks for critical paths

### Contributing Guidelines
1. Fork the repository
2. Create a feature branch
3. Implement changes with tests
4. Run full test suite
5. Submit pull request with detailed description

### Debugging
```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run with debugger
gdb ./hqts
(gdb) set args --config /etc/hqts/hqts.json --debug
(gdb) run
```

## 📚 Documentation

- **[API Reference](docs/api/README.md)** - Complete API documentation
- **[User Guide](docs/user-guide/README.md)** - Installation and usage guide
- **[Developer Guide](docs/developer-guide/README.md)** - Development documentation
- **[System Design](docs/design/architecture.md)** - Technical architecture
- **[Performance Guide](docs/performance/optimization.md)** - Performance tuning

## 🐛 Troubleshooting

### Common Issues

**High CPU Usage:**
```bash
# Check scheduling algorithm efficiency
hqts-cli monitor cpu --detail

# Optimize policy hierarchy
hqts-cli policy optimize --config /etc/hqts/hqts.json
```

**Memory Leaks:**
```bash
# Run with memory profiler
valgrind --tool=memcheck --leak-check=full ./hqts
```

**Network Performance:**
```bash
# Check DPDK configuration
hqts-cli hardware check

# Verify hugepage allocation
cat /proc/meminfo | grep Huge
```

## 📄 License

This software is licensed under a commercial license. See [LICENSE](LICENSE) file for details.

For licensing inquiries, contact: licensing@company.com

## 🤝 Support

- **Documentation**: [docs.company.com/hqts](https://docs.company.com/hqts)
- **Issues**: [GitHub Issues](https://github.com/company/hqts/issues)
- **Enterprise Support**: support@company.com
- **Community Forum**: [forum.company.com/hqts](https://forum.company.com/hqts)

## 🏆 Acknowledgments

- DPDK community for high-performance packet processing
- Boost libraries for efficient data structures
- Open source networking community for inspiration and guidance

---

**Note**: This is enterprise software designed for high-performance networking environments. Proper configuration and tuning are essential for optimal performance. Please consult the documentation and contact support for production deployments.
