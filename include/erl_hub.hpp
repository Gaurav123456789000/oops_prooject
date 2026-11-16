#ifndef ERL_HUB_HPP
#define ERL_HUB_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ReliefOps {
namespace Inventory {

enum class SupplyType { RationKit, TraumaKit, CleanWater, ThermalBlanket };
enum class MissionPriority { Routine = 1, High = 2, Critical = 3 };

std::string toString(SupplyType type);
SupplyType supplyTypeFromString(const std::string& value);
std::string toString(MissionPriority priority);

class CargoOverweightException : public std::exception {
public:
    const char* what() const noexcept override;
};

class FuelDepletionException : public std::exception {
public:
    const char* what() const noexcept override;
};

class DamagedCorridorException : public std::exception {
public:
    const char* what() const noexcept override;
};

class ReliefCargo {
public:
    ReliefCargo(SupplyType type = SupplyType::RationKit, int units = 0,
                double unitWeightKg = 1.0);

    SupplyType type() const noexcept;
    int units() const noexcept;
    double unitWeightKg() const noexcept;
    double totalWeightKg() const noexcept;

    friend std::ostream& operator<<(std::ostream& output, const ReliefCargo& cargo);
    friend std::istream& operator>>(std::istream& input, ReliefCargo& cargo);

private:
    SupplyType type_;
    int units_;
    double unitWeightKg_;
};

class DistributionDepot {
public:
    explicit DistributionDepot(std::string name);

    const std::string& name() const noexcept;
    int stockOf(SupplyType type) const noexcept;
    void setBarrier(SupplyType type, bool blocked);

    DistributionDepot& operator+=(const ReliefCargo& cargo);
    DistributionDepot& operator-=(const ReliefCargo& cargo);

private:
    friend class IncidentCommander;
    friend int allocateSupply(DistributionDepot& depot, SupplyType type, int requestedUnits);
    friend int allocateSupply(DistributionDepot& depot, SupplyType type,
                              MissionPriority priority, int requestedUnits);

    int withdraw_(SupplyType type, int requestedUnits, bool overrideBarrier);

    std::string name_;
    std::unordered_map<SupplyType, int> stock_;
    std::unordered_map<SupplyType, bool> barriers_;
};

class IncidentCommander {
public:
    int allocateCritical(DistributionDepot& depot, SupplyType type, int requestedUnits) const;
};

// Volume allocation and priority-based triage are separate overloads.
int allocateSupply(DistributionDepot& depot, SupplyType type, int requestedUnits);
int allocateSupply(DistributionDepot& depot, SupplyType type, MissionPriority priority,
                   int requestedUnits);

class Manifest {
public:
    Manifest();
    Manifest(const Manifest& other);
    Manifest(Manifest&& other) noexcept;
    Manifest& operator=(Manifest other) noexcept;
    ~Manifest();

    void swap(Manifest& other) noexcept;
    void add(const ReliefCargo& cargo);
    int remove(SupplyType type, int requestedUnits);
    int unitsOf(SupplyType type) const noexcept;
    std::size_t size() const noexcept;
    const ReliefCargo& at(std::size_t index) const;

    Manifest& operator+=(const ReliefCargo& cargo);
    Manifest& operator-=(const ReliefCargo& cargo);
    bool operator==(const Manifest& other) const noexcept;
    bool operator!=(const Manifest& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& output, const Manifest& manifest);
    friend std::istream& operator>>(std::istream& input, Manifest& manifest);

private:
    void reserve_(std::size_t newCapacity);

    ReliefCargo* items_;
    std::size_t size_;
    std::size_t capacity_;
};

class GPSBeacon {
public:
    explicit GPSBeacon(std::string callSign = "UNASSIGNED");
    const std::string& callSign() const noexcept;
private:
    std::string callSign_;
};

class EmergencyLifeSupportUnit {
public:
    explicit EmergencyLifeSupportUnit(int medicalPackCount = 0);
    int medicalPackCount() const noexcept;
private:
    int medicalPackCount_;
};

class ReliefVehicle {
public:
    ReliefVehicle(std::string id, double payloadLimitKg, double fuelRangeKm,
                  int medicalPackCount);
    virtual ~ReliefVehicle();
    ReliefVehicle(const ReliefVehicle& other);
    ReliefVehicle& operator=(const ReliefVehicle& other);

    const std::string& id() const noexcept;
    double payloadLimitKg() const noexcept;
    double currentPayloadKg() const noexcept;
    const Manifest& manifest() const noexcept;
    bool loadCargo(const ReliefCargo& cargo);
    void deliverCargo(SupplyType type, int units);

    static int activeVehicleCount() noexcept;
    static double totalDispatchedTonnageKg() noexcept;

    virtual std::string vehicleType() const = 0;
    virtual double evaluateRoute(double distanceKm, bool corridorDamaged) const = 0;
    virtual void writeBinary(std::ostream& output) const;

    friend std::ostream& operator<<(std::ostream& output, const ReliefVehicle& vehicle);

protected:
    double fuelRangeKm() const noexcept;

private:
    static int activeVehicleCount_;
    static double totalDispatchedTonnageKg_;

    std::string id_;
    double payloadLimitKg_;
    double fuelRangeKm_;
    double currentPayloadKg_;
    GPSBeacon gpsBeacon_;
    EmergencyLifeSupportUnit lifeSupport_;
    Manifest manifest_;
};

// The assignment names TransportVehicle in Unit II and ReliefVehicle in Unit III.
using TransportVehicle = ReliefVehicle;

class AerialTransport : virtual public ReliefVehicle {
public:
    AerialTransport(std::string id, double payloadLimitKg = 600.0,
                    double fuelRangeKm = 800.0, int medicalPackCount = 2);
    std::string vehicleType() const override;
    double evaluateRoute(double distanceKm, bool corridorDamaged) const override;
};

class GroundConvoy : virtual public ReliefVehicle {
public:
    GroundConvoy(std::string id, double payloadLimitKg = 5000.0,
                 double fuelRangeKm = 500.0, int medicalPackCount = 1);
    std::string vehicleType() const override;
    double evaluateRoute(double distanceKm, bool corridorDamaged) const override;
};

class AmphibiousCraft : public AerialTransport, public GroundConvoy {
public:
    AmphibiousCraft(std::string id, double payloadLimitKg = 1800.0,
                    double fuelRangeKm = 650.0, int medicalPackCount = 3);
    std::string vehicleType() const override;
    double evaluateRoute(double distanceKm, bool corridorDamaged) const override;
};

} // namespace Inventory

namespace Logistics {

using Inventory::MissionPriority;
using Inventory::SupplyType;

struct MissionRequest {
    std::string incidentId;
    SupplyType supply;
    int units;
    MissionPriority priority;
};

struct UrgencyFirst {
    bool operator()(const MissionRequest& left, const MissionRequest& right) const noexcept;
};

template <typename T, typename Compare = std::less<T>>
class PriorityDispatchQueue {
public:
    void push(const T& value) {
        items_.push_back(value);
        std::sort(items_.begin(), items_.end(), compare_);
    }

    bool empty() const noexcept { return items_.empty(); }
    std::size_t size() const noexcept { return items_.size(); }

    T pop() {
        if (items_.empty()) {
            throw std::out_of_range("Dispatch queue is empty");
        }
        T next = items_.front();
        items_.erase(items_.begin());
        return next;
    }

private:
    std::vector<T> items_;
    Compare compare_{};
};

} // namespace Logistics

namespace Inventory {

struct ManifestAudit {
    std::string vehicleId;
    std::string vehicleType;
    std::vector<ReliefCargo> cargo;
};

struct AuditSnapshot {
    std::vector<ManifestAudit> manifests;
    std::vector<std::string> transitLogs;
};

struct TableHeader {};
struct TableRule {};
struct ManifestTableHeader {};
std::ostream& operator<<(std::ostream& output, TableHeader);
std::ostream& operator<<(std::ostream& output, TableRule);
std::ostream& operator<<(std::ostream& output, ManifestTableHeader);

// Inline utility required for quick distance and payload calculations.
inline double estimatePayloadKg(int units, double unitWeightKg) noexcept {
    return static_cast<double>(units) * unitWeightKg;
}

inline double estimateRouteKm(double latitudeA, double longitudeA,
                              double latitudeB, double longitudeB) noexcept {
    const double latitudeDelta = latitudeB - latitudeA;
    const double longitudeDelta = longitudeB - longitudeA;
    return 111.0 * std::sqrt(latitudeDelta * latitudeDelta +
                             longitudeDelta * longitudeDelta);
}

void saveAuditAtomically(const std::vector<ReliefVehicle*>& fleet,
                         const std::vector<std::string>& transitLogs,
                         const std::string& filePath);
AuditSnapshot restoreAudit(const std::string& filePath);

} // namespace Inventory
} // namespace ReliefOps

#endif
