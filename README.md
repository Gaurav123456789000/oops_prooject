# ERL-HUB

**Post-Earthquake Relief Logistics Hub** is a C++17 console project for managing emergency supply stock, prioritizing relief requests, loading transport vehicles, checking routes, recording deliveries, and saving audit records.

The project is designed around Option 2 of the OOP assignment. It uses only the C++ standard library; no third-party packages are required.

## Run modes

At startup, choose one of two modes:

| Choice | Mode | What it does |
| --- | --- | --- |
| `1` | Custom scenario | Enter depot stock, mission requests, vehicle details, and route conditions. The program allocates stock and shows the resulting dispatch, route, and manifest tables. |
| `2` | Sample scenario | Run the included North Field Depot walkthrough with fixed stock, missions, vehicles, route conditions, and safety checks. |

Both modes save and restore vehicle manifests and transit logs in `erl_hub_audit.bin` in the current working directory. The file is written to a temporary file and then replaced after a successful write.

## Build and run

### CMake

From the repository folder:

```sh
cmake -S . -B build
cmake --build build
```

Run the resulting executable:

```sh
# Linux / macOS
./build/erl_hub

# Windows Visual Studio build (choose Debug or Release as appropriate)
./build/Debug/erl_hub.exe

# Windows MinGW build
./build/erl_hub.exe
```

### Compile the standalone source

The repository also contains [erl_hub_single.cpp](erl_hub_single.cpp), which combines the project into one file for compilers that do not use CMake:

```sh
g++ -std=c++17 erl_hub_single.cpp -o erl_hub
./erl_hub
```

For PowerShell on Windows:

```powershell
g++ -std=c++17 erl_hub_single.cpp -o erl_hub.exe
.\erl_hub.exe
```

## Entering a custom scenario

Select **1** at startup. The prompts are grouped into inventory, mission, vehicle, route, and delivery sections.

### 1. Depot inventory

Enter a one-word depot name and stock quantities for each supply:

| Supply number | Supply |
| --- | --- |
| `1` | Ration kit |
| `2` | Trauma kit |
| `3` | Clean water |
| `4` | Thermal blanket |

Then indicate whether trauma-kit stock is behind an access barrier (`0` = no, `1` = yes). A critical request can use the friend `IncidentCommander` override to access barrier-restricted stock.

### 2. Mission requests

Enter between 1 and 12 mission rows. Each row has this format:

```text
INCIDENT_ID SUPPLY_NUMBER UNITS PRIORITY_NUMBER
```

For example:

```text
EQ-101 2 8 3
```

This requests 8 trauma kits for incident EQ-101 at critical priority.

| Priority number | Priority |
| --- | --- |
| `1` | Routine |
| `2` | High |
| `3` | Critical |

The dispatch queue sorts critical requests first, then high, then routine. For equal priorities, trauma-kit requests are prioritized, followed by larger requests.

### 3. Vehicles

Enter one row for each vehicle in this order: ground convoy, aerial transport, amphibious craft.

```text
VEHICLE_ID PAYLOAD_LIMIT_KG FUEL_RANGE_KM
```

Example:

```text
TRK-01 2400 500
```

### 4. Route and delivery

Enter the route distance in kilometers and whether the road corridor is damaged (`0` = no, `1` = yes). The program evaluates the route for all three vehicles.

After reviewing the manifests, you can choose whether to record a completed delivery. If so, select a vehicle, a manifest row, and the number of units delivered. The program updates the manifest and delivered-tonnage total.

## Sample scenario walkthrough

Choose **2** to run the repeatable North Field Depot example.

### Depot stock and allocations

The sample depot starts with:

| Supply | Initial stock | Allocated |
| --- | ---: | ---: |
| Ration kits | 60 | 30 |
| Trauma kits | 20 | 8 |
| Clean water | 80 | 15 |
| Thermal blankets | 25 | 0 |

Trauma kits are marked as barrier-restricted. The critical medical request still receives 8 kits through the emergency triage override. The other supplies are allocated through the volume and priority overloads.

### Mission order

| Queue position | Incident | Priority | Request |
| ---: | --- | --- | --- |
| 1 | EQ-101 | Critical | 8 trauma kits |
| 2 | EQ-103 | High | 15 clean-water units |
| 3 | EQ-102 | Routine | 30 ration kits |

### Vehicle route assessment

The sample route is approximately **238.3 km**. The road corridor is marked damaged.

| Vehicle | Sample load | Route result |
| --- | ---: | --- |
| TRK-01 ground convoy | 36 kg | Rejected because the corridor is damaged |
| AIR-01 aerial transport | 20 kg | Accepted; estimated 1.32 hours |
| AMP-01 amphibious craft | 15 kg | Accepted; estimated 2.65 hours |

The safety section also attempts an overweight aircraft load and a 900 km aircraft route to demonstrate the cargo-weight and fuel-range exceptions.

### Delivery and saved records

The sample records delivery of 10 ration kits. At 1.2 kg per kit, that is 12 kg, or **0.012 metric tonnes**. It then restores three vehicle manifests and five transit-log entries from the binary audit file.

## OOP requirements demonstrated

| Assignment topic | Project implementation |
| --- | --- |
| Namespaces and core C++ | `ReliefOps::Inventory` contains stock and vehicle types; `ReliefOps::Logistics` contains mission requests and the dispatch queue. Supply allocation is overloaded for quantity and priority. Inline calculation helpers, reference parameters, reference-returning depot operators, and the friend `IncidentCommander` are included. |
| Classes, memory, and operators | Encapsulated `ReliefCargo`, `DistributionDepot`, and `ReliefVehicle` types; static operational-vehicle and delivered-tonnage totals; dynamically allocated manifests with deep-copy construction and cleanup; overloaded `+=`, `-=`, `==`, `<<`, and `>>`. `TransportVehicle` is provided as an alias for the shared vehicle base named in Unit II. |
| Inheritance and polymorphism | `AerialTransport` and `GroundConvoy` virtually inherit `ReliefVehicle`; `AmphibiousCraft` inherits both branches. Vehicles compose `GPSBeacon` and `EmergencyLifeSupportUnit`, and route evaluation runs polymorphically through `ReliefVehicle*`. |
| Exceptions, files, and STL | Custom cargo-weight, fuel-range, and damaged-corridor exceptions; binary manifest and transit-log persistence with temporary-file replacement; `PriorityDispatchQueue<T>`, `vector`, `unordered_map`, `sort`, an urgency functor, and table stream manipulators. |

## Project files

```text
include/erl_hub.hpp   Domain types, interfaces, and queue template
src/erl_hub.cpp      Inventory, vehicle, route, and persistence implementations
src/main.cpp         Custom and sample console flows
erl_hub_single.cpp   Standalone combined source file
CMakeLists.txt       CMake build definition
README.md            Build instructions and project walkthrough
```

## Compiler

The project requires a C++17 compiler and CMake 3.16 or newer for the CMake build. The standalone version can be compiled directly with `g++ -std=c++17`.
