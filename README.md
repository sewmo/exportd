# Overview
**exportd is a simplistic metric exporter for exposing machine metrics in *NIX systems through HTTP endpoints.**  
The exportd metric exporter is designed to actively monitor various system metrics such as component temperatures, active processes and their resource usage, filesystem statistics, and more.  
Support for Docker integration is provided. Alternatively, the executable can be ran as is, and the process shall ran in the background (daemon).  
# Dependencies
Building the exportd binary requires no more dependencies than the C standard library (glibc) and common Linux utilities.  
# Installation
Installation requires two steps:
## 1. Installing Build Dependencies
Install the dependencies listed in the previous section.
## 2. Installing exportd
Clone the repository, move into the newly created directory, and run the **make build**:
```
git clone https://github.com/sewmo/exportd.git
cd exportd
make build
./exportd 
```
Configuration flags can be listed by running the following:
```
./exportd -h
```
