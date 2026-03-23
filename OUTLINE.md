# OceanBase Project Outline

## Project Overview

OceanBase Database is a distributed relational database developed by Ant Group. It is built on a common server cluster using the Paxos protocol, providing high availability and linear scalability.

## Repository Structure

### Core Directories

#### `/src` - Source Code
The main source code directory containing the core implementation of OceanBase Database.

#### `/deps` - Dependencies
External dependencies and libraries required by OceanBase:
- `oblib` - OceanBase library
- `easy` - Network library
- `ussl-hook` - SSL hook implementation

#### `/docs` - Documentation
Developer documentation in both English and Chinese:
- `/docs/docs/en/` - English documentation
- `/docs/docs/zh/` - Chinese documentation
- Uses MkDocs for documentation site generation

#### `/unittest` - Unit Tests
Unit test suites for testing individual components

#### `/mittest` - Mini Tests
Mini test framework and test cases

#### `/test` - Test Files
Additional test files and test data

#### `/tools` - Development Tools
Various tools for development and maintenance

#### `/script` - Scripts
Build scripts, deployment scripts, and utility scripts

#### `/cmake` - CMake Configuration
CMake build system configuration files

#### `/package` - Packaging
Packaging configuration for distribution

#### `/rpm` - RPM Packaging
RPM package building configuration

#### `/profile` - Profiling
Profiling tools and configurations

#### `/images` - Images
Project logos and other image assets

## Documentation Structure

### Getting Started
1. [Install toolchain](docs/docs/en/toolchain.md)
2. [Get the code, build and run](docs/docs/en/build-and-run.md)
3. [Set up an IDE](docs/docs/en/ide-settings.md)
4. [Coding Conventions](docs/docs/en/coding-convension.md)
5. [Write and run unit tests](docs/docs/en/unittest.md)
6. [Running MySQL test](docs/docs/en/mysqltest.md)
7. [Debug](docs/docs/en/debug.md)
8. [Commit code and submit a pull request](docs/docs/en/contributing.md)

### Design and Implementation
1. [Logging System](docs/docs/en/logging.md)
2. [Memory Management](docs/docs/en/memory.md)
3. [Basic Data Structures](docs/docs/en/container.md)
4. [Architecture](docs/docs/en/architecture.md)
5. [Coding Standard](docs/docs/en/coding_standard.md)

## Key Features

- **Transparent Scalability**: Supports 1,500 nodes, PB data, and trillion-row records
- **Ultra-fast Performance**: TPC-C 707M tmpC and TPC-H 15.26M QphH @30000GB
- **Cost Efficiency**: Saves 70%-90% of storage costs
- **Real-time Analytics**: HTAP support without additional cost
- **Continuous Availability**: RPO = 0, RTO < 8s
- **MySQL Compatible**: Easy migration from MySQL

## Build System

The project uses CMake as its build system:
- Main configuration: `CMakeLists.txt`
- Build script: `build.sh`
- Platform support: Linux (primary), with Docker and Kubernetes deployment options

## Testing Framework

### Unit Tests
Located in `/unittest` directory, covering individual component testing

### MySQL Tests
Integration tests using MySQL test framework, documented in `mysqltest.md`

### Mini Tests
Specialized test framework in `/mittest` for specific scenarios

## Development Workflow

1. Fork the repository
2. Clone your fork
3. Create a feature branch
4. Make changes and commit
5. Push to your fork
6. Create a pull request

### CI/CD
- **Compile Check**: Builds on CentOS and Ubuntu
- **Farm Check**: Runs unit tests and MySQL test cases

## Contributing

Contributions are welcome! Please read:
- [CONTRIBUTING.md](CONTRIBUTING.md) - Contribution guidelines
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) - Community code of conduct

## License

OceanBase Database is licensed under the Mulan Public License, Version 2. See [LICENSE](LICENSE) for details.

## Community

- [Discord](https://discord.gg/74cF8vbNEs)
- [Stack Overflow](https://stackoverflow.com/questions/tagged/oceanbase)
- [Chinese User Forum](https://ask.oceanbase.com/)
- DingTalk Group: 33254054
- WeChat: OBCE666

## Additional Resources

- Official website: https://oceanbase.com
- English documentation: https://en.oceanbase.com/docs/oceanbase-database
- Chinese documentation: https://www.oceanbase.com/docs/oceanbase-database-cn
- User documentation: [oceanbase-doc](https://github.com/oceanbase/oceanbase-doc)
- Developer guide: https://oceanbase.github.io/oceanbase

## Repository Information

- Main repository: https://github.com/oceanbase/oceanbase
- Primary branch: master
- Development branch: develop
