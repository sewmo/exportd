# Overview
**exportd is a simplistic metric exporter for exposing machine metrics from UNIX-like systems through HTTP endpoints.**  
The exportd metric exporter is designed to actively monitor various system metrics such as component temperatures, active processes and their resource usage, filesystem statistics, and more.  
Support for Docker integration is provided. Alternatively, the executable can be ran as is, and the process shall ran in the background (daemon).  
The exporter is designed to be compatible with the Prometheus database.  
# Dependencies
Building the exportd binary requires no more dependencies than the C standard library (glibc) and common Linux utilities.  
# Installation
Installation requires two simple steps:
### 1. Installing Build Dependencies
Install the dependencies listed in the previous section.
### 2. Installing exportd
Clone the repository, move into the newly created repository, and run the **make build** command:
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
# Technical Overview
Once the executable is ran, a background process known as a daemon on Linux systems is created. The daemon fetches various system and OS metrics and exposes them to an HTTP endpoint using a simple web server.  
Your database then periodically checks the HTTP endpoint for metric updates. The project was designed with the Prometheus time-series database in mind.  
The web server that creates the HTTP endpoint is fully built by myself. The project uses no external dependencies or utilities, such as the Apache web server. The only dependencies are the standard Linux utilities that ship with most distributions.  
Moreover, the exporter can be ran inside a **Docker container** which provides an isolated environment. This, however, necessites the specification of certain flags in order to give the application access to host namespaces rather than container namespaces.  
Logs that document the web server's events are provided, as well as logs for the system that is responsible for fetching metrics.  
# Notes
Support for TLS will be provided once the web server and metric exporter have been finalized.
